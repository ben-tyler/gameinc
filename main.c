#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#pragma region Types

#define MAX_ENEMIES 5
#define MAX_BULLETS 10
#define MAX_NPCS 3
#define MAX_PARTICLES 100
#define ENEMY_RESPAWN_TIME 2.0f
#define SWORD_COOLDOWN 0.5f    
#define SWORD_DURATION 0.2f    
#define PARTICLE_LIFESPAN 0.5f  
#define MAP_WIDTH 32
#define MAP_HEIGHT 18
#define MAX_CRATES 10
#define MAX_GOLD 10
#define SWORD_WIDTH 10
#define SWORD_HEIGHT 30
#define MAX_DAMAGE_PARTICLES 100
#define GRAVITY 100.0f 
#define INITIAL_GOLD_SPEED 200.0f 


typedef struct DamageParticle {
	Vector2 position;
	Vector2 velocity;
	float lifetime; 
	int damageAmount;
	bool active;
	Color color;
} DamageParticle;

typedef struct Particle {
	Vector2 position;
	Vector2 velocity;
	float lifespan;
	Color color;
	bool active;
} Particle;



typedef struct Crate {
	Vector2 position;
	int size;
	int health;
	bool active;
	Color color;
	bool hasGold;
} Crate;

typedef struct Gold {
	Vector2 position;
	int size;
	bool active;
	Vector2 velocity;  
} Gold;

typedef struct Player {
	Vector2 position;
	Vector2 direction;
	float speed;
	int size;
	int health;
	Color color;
	bool isMoving;
} Player;

typedef struct Enemy {
	Vector2 position;
	float speed;
	int size;
	int health;
	bool active;
	Color color;
	float attackCooldown;
	float damageTextTimer;
} Enemy;

typedef struct Bullet {
	Vector2 position;
	Vector2 direction;
	float speed;
	int size;
	bool active;
	Color color;
} Bullet;

typedef struct NPC {
	Vector2 position;
	int size;
	const char* dialog;
	bool active;
	Color color;
	int dialogState;
	bool interact;
} NPC;

typedef struct Sword {
	Vector2 position;
	Vector2 size;
	Color color;
	bool active;
	float cooldown;
	float duration;
} Sword;


#pragma endregion


const char* npc[2][8] = {
	{
		"        ",
		"    X   ",
		"   XX   ",
		"  XXXX  ",
		"  XX    ",
		"    XX  ",
		"        ",
		"        "
	},
	{
		"        ",
		"   X    ",
		"   XX   ",
		"  XXXX  ",
		"    XX  ",
		"  XX    ",
		"        ",
		"        "
	}
};

const char* enemy[2][8] = {
	{
		"        ",
		"        ",
		"  X  X  ",
		"  XXXX  ",
		"   X X  ",
		"  X  X  ",
		"        ",
		"        "
	},
	{
		"        ",
		"        ",
		"    XX  ",
		"  XXXX  ",
		"  XX X  ",
		"  XX X  ",
		"        ",
		"        "
	}
};

const char* playerIdle[2][8] = {
	{
		"        ",
		"        ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"  XX XX ",
		"        "
	},
	{
		"        ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"  XX XX ",
		"        "
	}
};

const char* playerWalking[2][8] = {
	{
		"        ",
		"   XX   ",
		"   XX   ",
		"  XXXX  ",
		"  XXXX  ",
		"   XX   ",
		"  XX XX ",
		"        "
	},
	{
		"        ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"   XX   ",
		"  XXXX  ",
		"        "
	}
};

char map[MAP_HEIGHT][MAP_WIDTH + 1] = {
	"################################",
	"#...........#..................#",
	"#...........#..................#",
	"#...........##########.........#",
	"#...........#.......#..........#",
	"#......######.......#..........#",
	"#...................#..........#",
	"#...................#..........#",
	"#...........#########..........#",
	"#...........##.................#",
	"#..............................#",
	"#############..................#",
	"#..............................#",
	"#..............................#",
	"#..............####............#",
	"#..............####............#",
	"#..............####............#",
	"################################"
};

typedef enum {
	StageOneSetup,
	StageOne,
	StageTwoSetup,
	StageTwo,
	StageThree
} GameStage;

#define MAX_NODES 1000 // Max number of nodes the enemy can visit during A*

typedef struct Node {
	int x, y;  // Position on the grid
	float gCost, hCost, fCost;  // g = from start, h = to target, f = g + h
	struct Node* parent;  // Pointer to parent node for reconstructing the path
	bool walkable;  // Is this tile walkable (not an obstacle)?
} Node;

// Check if a given position is walkable and within map bounds
bool isWalkable(int x, int y) {
	return (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT && map[y][x] != '#');
}

// Calculate the heuristic (Manhattan distance for a grid)
float heuristic(int x1, int y1, int x2, int y2) {
	return fabsf(x2 - x1) + fabsf(y2 - y1);
}

// Compare nodes by fCost for the priority queue
int compareNodes(const void* a, const void* b) {
	Node* nodeA = (Node*)a;
	Node* nodeB = (Node*)b;
	return (nodeA->fCost > nodeB->fCost) - (nodeA->fCost < nodeB->fCost);
}

Node* findPath(Vector2 startPos, Vector2 targetPos, int tileSize) {
	Node nodes[MAP_HEIGHT][MAP_WIDTH];
	Node* openList[MAX_NODES];  // Nodes to evaluate
	int openListCount = 0;

	bool closedList[MAP_HEIGHT][MAP_WIDTH] = { false };  // Nodes already evaluated

	// Initialize nodes
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			nodes[y][x] = (Node){ x, y, 0, 0, 0, NULL, isWalkable(x, y) };
		}
	}

	// Add start node to the open list
	Node* startNode = &nodes[(int)(startPos.y / tileSize)][(int)(startPos.x / tileSize)];
	Node* targetNode = &nodes[(int)(targetPos.y / tileSize)][(int)(targetPos.x / tileSize)];

	openList[openListCount++] = startNode;

	while (openListCount > 0) {
		// Sort the open list by fCost (lowest cost first)
		qsort(openList, openListCount, sizeof(Node*), compareNodes);

		// Get the node with the lowest fCost
		Node* currentNode = openList[0];

		// Remove the node from the open list
		for (int i = 0; i < openListCount - 1; i++) openList[i] = openList[i + 1];
		openListCount--;

		// Add current node to the closed list
		closedList[currentNode->y][currentNode->x] = true;

		// Check if we've reached the target
		if (currentNode == targetNode) {
			return currentNode;  // Path found, return the last node to trace the path
		}

		// Check neighboring nodes (up, down, left, right)
		Node* neighbors[4];
		int neighborCount = 0;

		if (currentNode->x > 0) neighbors[neighborCount++] = &nodes[currentNode->y][currentNode->x - 1];  // Left
		if (currentNode->x < MAP_WIDTH - 1) neighbors[neighborCount++] = &nodes[currentNode->y][currentNode->x + 1];  // Right
		if (currentNode->y > 0) neighbors[neighborCount++] = &nodes[currentNode->y - 1][currentNode->x];  // Up
		if (currentNode->y < MAP_HEIGHT - 1) neighbors[neighborCount++] = &nodes[currentNode->y + 1][currentNode->x];  // Down

		for (int i = 0; i < neighborCount; i++) {
			Node* neighbor = neighbors[i];

			if (!neighbor->walkable || closedList[neighbor->y][neighbor->x]) continue;  // Skip if it's not walkable or already evaluated

			float newGCost = currentNode->gCost + 1;  // Assume uniform cost for each step
			bool isInOpenList = false;

			for (int j = 0; j < openListCount; j++) {
				if (openList[j] == neighbor) {
					isInOpenList = true;
					break;
				}
			}

			if (newGCost < neighbor->gCost || !isInOpenList) {
				neighbor->gCost = newGCost;
				neighbor->hCost = heuristic(neighbor->x, neighbor->y, targetNode->x, targetNode->y);
				neighbor->fCost = neighbor->gCost + neighbor->hCost;
				neighbor->parent = currentNode;

				if (!isInOpenList) {
					openList[openListCount++] = neighbor;  // Add to open list if not already present
				}
			}
		}
	}

	return NULL;  // No path found
}

Vector2 getNextPathPosition(Node* targetNode, Enemy* enemy, int tileSize) {
	Node* currentNode = targetNode;

	// Backtrack from target node to get the next position
	while (currentNode->parent != NULL && currentNode->parent->parent != NULL) {
		currentNode = currentNode->parent;  // Get second-to-last node in the path
	}

	return (Vector2) { currentNode->x* tileSize, currentNode->y* tileSize };
}


typedef struct GameModel {
	Player player;
	Sword sword;
	Enemy enemies[MAX_ENEMIES];
	Particle particles[MAX_PARTICLES];
	Bullet bullets[MAX_BULLETS];
	Crate crates[MAX_CRATES];
	Gold gold[MAX_GOLD];
	float enemySpawnTimer;
	NPC npcs[MAX_NPCS];
	const char* activeDialog;
	int goldCollected;
	DamageParticle damageParticles[MAX_DAMAGE_PARTICLES];
	GameStage stage;
	int killCount;
} GameModel;



////////// 
#pragma region INIT

int animationFrame = 0;
float animationTimer = 0.0f;
const float frameDuration = 0.3f;

void spawnDamageParticle(GameModel* model, Vector2 position, int damageAmount, Color color) {
	for (int i = 0; i < MAX_DAMAGE_PARTICLES; i++) {
		if (!model->damageParticles[i].active) {
			model->damageParticles[i].active = true;
			model->damageParticles[i].position = position;
			model->damageParticles[i].velocity = (Vector2){ 0, -50 }; // Move the particle upward
			model->damageParticles[i].lifetime = 1.0f; // Lasts for 1 second
			model->damageParticles[i].damageAmount = damageAmount;
			model->damageParticles[i].color = color;
			break;
		}
	}
}

void spawnParticles(Particle particles[], Vector2 position, int count, Color color) {
	for (int i = 0; i < MAX_PARTICLES && count > 0; i++) {
		if (!particles[i].active) {
			particles[i].position = position;
			// Random velocity
			float angle = (float)(rand() % 360) * DEG2RAD;
			float speed = (float)(rand() % 100) / 50.0f;
			particles[i].velocity = (Vector2){ cosf(angle) * speed, sinf(angle) * speed };
			particles[i].lifespan = PARTICLE_LIFESPAN;
			particles[i].color = color;
			particles[i].active = true;
			count--;
		}
	}
}

void spawnCrates(Crate crates[], int tileSize) {
	for (int i = 0; i < MAX_CRATES; i++) {
		if (!crates[i].active) {
			crates[i].position = (Vector2){ (rand() % (MAP_WIDTH - 2) + 1) * tileSize, (rand() % (MAP_HEIGHT - 2) + 1) * tileSize };
			crates[i].size = tileSize;
			crates[i].health = 2;
			crates[i].active = true;
			crates[i].color = BROWN;
			crates[i].hasGold = true;
		}
	}
}

GameModel spawnEnemies(GameModel model, float deltaTime, int tileSize)
{
	model.enemySpawnTimer += deltaTime;
	if (model.enemySpawnTimer >= ENEMY_RESPAWN_TIME) {
		model.enemySpawnTimer = 0.0f;
		for (int i = 0; i < MAX_ENEMIES; i++) {
			if (!model.enemies[i].active) {
				Vector2 spawnPos;
				bool validSpawn = false;
				while (!validSpawn) {
					spawnPos = (Vector2){ (rand() % MAP_WIDTH) * tileSize, (rand() % MAP_HEIGHT) * tileSize };
					int spawnMapX = spawnPos.x / tileSize;
					int spawnMapY = spawnPos.y / tileSize;
					if (spawnMapY >= 0 && spawnMapY < MAP_HEIGHT && spawnMapX >= 0 && spawnMapX < MAP_WIDTH) {
						if (map[spawnMapY][spawnMapX] != '#') {
							validSpawn = true;
							Rectangle spawnRect = { spawnPos.x, spawnPos.y, model.enemies[i].size, model.enemies[i].size };
							Rectangle playerRect = { model.player.position.x, model.player.position.y, model.player.size, model.player.size };
							if (CheckCollisionRecs(spawnRect, playerRect)) {
								validSpawn = false;
							}
						}
					}
				}
				model.enemies[i].position = spawnPos;
				model.enemies[i].active = true;
				model.enemies[i].health = 3;
				break;
			}
		}
	}
	return model;
}

GameModel spawnGold(GameModel model, Vector2 cratePosition, int tileSize) {
	for (int g = 0; g < MAX_GOLD; g++) {
		if (!model.gold[g].active) {
			float randomOffsetX = (rand() % 20 - 10) * 0.1f; // Random offset between -1.0 and 1.0
			float randomOffsetY = (rand() % 20 - 10) * 0.1f;

			model.gold[g].position = cratePosition;

			// Set initial velocity to simulate "falling out"
			model.gold[g].velocity = (Vector2){
				randomOffsetX * 500.0f, randomOffsetY * 500.0f
			};

			model.gold[g].size = tileSize / 2;
			model.gold[g].active = true;
			break;
		}
	}
	return model;
}

GameModel setup(int tileSize)
{
	GameModel model =
	{ .player = (Player)
		 {.position = (Vector2){ 2 * tileSize, 2 * tileSize }
		 , .direction = (Vector2){ 0, -1 }
		 , .speed = 200.0f
		 , .size = 50
		 , .health = 10
		 , .color = BLUE
		}
	, .sword = (Sword)
		{.size = (Vector2){40, 10}
		, .color = DARKBROWN
		, .active = false
		, .cooldown = 0.0f
		, .duration = 0.0f
		}
	, .enemies = {0}
	, .particles = { 0 }
	, .bullets = { 0 }
	, .enemySpawnTimer = 0.0f
	, .npcs = { 0 }
	, .activeDialog = NULL
	, .goldCollected = 0
	, .crates = {0}
	, .gold = {0}
	, .stage = StageOne
	};

	for (int i = 0; i < MAX_NPCS; i++) {
		model.npcs[i].position = (Vector2){ (4 + i * 2) * tileSize, 2 * tileSize };
		model.npcs[i].size = tileSize;
		model.npcs[i].dialog = "";//i == 0 ? "Hello, brave warrior!" : i == 1 ? "Beware of the enemies ahead!" : "You must find the sacred relic.";
		model.npcs[i].active = false;
		model.npcs[i].color = GREEN;
	}

	for (int i = 0; i < MAX_ENEMIES; i++) {
		model.enemies[i].speed = 100.0f;
		model.enemies[i].size = tileSize;
		model.enemies[i].health = 1;
		model.enemies[i].active = false;
		model.enemies[i].color = RED;
		model.enemies[i].attackCooldown = 0.0f;
		model.enemies[i].damageTextTimer = 0.0f;
	}

	for (int i = 0; i < MAX_BULLETS; i++) {
		model.bullets[i].speed = 400.0f;
		model.bullets[i].size = 10;
		model.bullets[i].active = false;
		model.bullets[i].color = BLACK;
	}

	spawnCrates(model.crates, tileSize);
	return model;
}
#pragma endregion

#pragma region Update

GameModel moveNPCToPlayer(GameModel model, float deltaTime, float npcSpeed, float stoppingDistance) {
	// Access the NPC and the player's position
	Vector2 npcPos = model.npcs[0].position; // NPC's current position
	Vector2 playerPos = model.player.position; // Player's current position

	// Calculate the direction vector from the NPC to the player
	Vector2 direction = {
		playerPos.x - npcPos.x,
		playerPos.y - npcPos.y
	};

	// Calculate the distance between the NPC and the player
	float distance = sqrtf(direction.x * direction.x + direction.y * direction.y);

	// Move the NPC only if it's farther than the stopping distance
	if (distance > stoppingDistance) {
		// Normalize the direction vector
		direction.x /= distance;
		direction.y /= distance;

		// Update the NPC's position by moving it towards the player
		npcPos.x += direction.x * npcSpeed * deltaTime;
		npcPos.y += direction.y * npcSpeed * deltaTime;

		// Assign the updated position back to the NPC in the model
		model.npcs[0].position = npcPos;
	}

	return model;
}

GameModel updateStage(GameModel model, float deltaTime, int tileSize) {
	switch (model.stage) {
	case StageOneSetup:
		// NPC dialog setup for stage one
		
		model.stage = StageOne;
		break;

	case StageOne:
		
		model.npcs[0].active = true;
		if (model.npcs[0].dialogState == 0) {
			model.npcs[0].dialog = "Hey There! I need five gold, can you bring it? ( Press E to cont.)";
			model.npcs[0].dialogState++;
		}
		if (model.npcs[0].interact && model.npcs[0].dialogState == 1) {
			model.npcs[0].dialog = "go get it ( Press E to giv)";
			model.npcs[0].dialogState++;
		}
		else if (model.goldCollected >= 1 && model.npcs[0].interact && model.npcs[0].dialogState == 2) {
			model.npcs[0].dialog = "Holy shit the gold!, we should not have teken i ( Press E to cont.)t";
			model.npcs[0].dialogState++;
			model.goldCollected -= 5;
		}
		else if (model.npcs[0].interact && model.npcs[0].dialogState == 2) {
			model.npcs[0].dialog = "you dont have the gold ( Press E to cont.)";
			model.npcs[0].dialogState = 1;
		}
		else if ( model.npcs[0].interact && model.npcs[0].dialogState == 3) {
			model.killCount = 0;
			model.stage = StageTwoSetup;
			
		}
		model.npcs[0].interact = false;
		break;

	case StageTwoSetup:
		// Transition to stage two
		//model.npcs[0].active = false; // Disable the NPC
		model.npcs[0].active = true;
		if (model.killCount > 5)
		{
			model.npcs[0].dialog = "Good job handleing those bad guys";
			model = moveNPCToPlayer(model, deltaTime, 100.0f, 100);

		}
		else
		{
			model.npcs[0].dialog = "We are under attack!";
			model = moveNPCToPlayer(model, deltaTime, 100.0f, 100);
			model = spawnEnemies(model, deltaTime, tileSize);
		}
		break;

	case StageTwo:
		// Enemy spawning logic
		
		break;

	case StageThree:
		// Future stages can be added here
		break;
	}
	return model;
}

void updateParticles(Particle particles[], float deltaTime) {
	for (int i = 0; i < MAX_PARTICLES; i++) {
		if (particles[i].active) {
			particles[i].position.x += particles[i].velocity.x * deltaTime * 200.0f;
			particles[i].position.y += particles[i].velocity.y * deltaTime * 200.0f;
			particles[i].lifespan -= deltaTime;
			if (particles[i].lifespan <= 0.0f) {
				particles[i].active = false;
			}
			else {
				float alpha = particles[i].lifespan / PARTICLE_LIFESPAN;
				particles[i].color.a = (unsigned char)(alpha * 255);
			}
		}
	}
}

GameModel updateCrates(GameModel model, int tileSize) {
	for (int i = 0; i < MAX_CRATES; i++) {
		if (model.crates[i].active) {
			for (int j = 0; j < MAX_BULLETS; j++) {
				if (model.bullets[j].active && CheckCollisionRecs(
					(Rectangle) {
					model.crates[i].position.x, model.crates[i].position.y, model.crates[i].size, model.crates[i].size
				},
					(Rectangle) {
					model.bullets[j].position.x, model.bullets[j].position.y, model.bullets[j].size, model.bullets[j].size
				})) {

					model.crates[i].health--;
					spawnParticles(model.particles, (Vector2) { model.crates[i].position.x + model.crates[i].size / 2, model.crates[i].position.y + model.crates[i].size / 2 }, 10, BROWN);

					model.bullets[j].active = false;
					if (model.crates[i].health <= 0) {
						model.crates[i].active = false;
						for (int g = 0; g < MAX_GOLD; g++) {
							if (!model.gold[g].active) {
								model = spawnGold(model, model.crates[i].position, tileSize);
								break;
							}
						}
					}
					break;
				}
			}
			if (model.sword.active && CheckCollisionRecs(
				(Rectangle) {
				model.crates[i].position.x, model.crates[i].position.y, model.crates[i].size, model.crates[i].size
			},
				(Rectangle) {
				model.sword.position.x, model.sword.position.y, model.sword.size.x, model.sword.size.y
			})) {
				spawnParticles(model.particles, (Vector2) { model.crates[i].position.x + model.crates[i].size / 2, model.crates[i].position.y + model.crates[i].size / 2 }, 5, BROWN);
				model.crates[i].health--;
				if (model.crates[i].health <= 0) {
					model.crates[i].active = false;
					for (int g = 0; g < MAX_GOLD; g++) {
						if (!model.gold[g].active) {
							model = spawnGold(model, model.crates[i].position, tileSize);
							break;
						}
					}
				}
			}
		}
	}
	return model;
}

GameModel updateGold(GameModel model, float deltaTime) {
	for (int i = 0; i < MAX_GOLD; i++) {
		if (model.gold[i].active) {
			// Update position based on velocity
			model.gold[i].position.x += model.gold[i].velocity.x * deltaTime;
			model.gold[i].position.y += model.gold[i].velocity.y * deltaTime;

			// Gradually slow down the gold pieces
			model.gold[i].velocity.x *= 0.9f;
			model.gold[i].velocity.y *= 0.9f;

			// Stop the gold after it slows down enough
			if (fabs(model.gold[i].velocity.x) < 0.1f && fabs(model.gold[i].velocity.y) < 0.1f) {
				model.gold[i].velocity.x = 0.0f;
				model.gold[i].velocity.y = 0.0f;
			}
		}
		if (model.gold[i].active && CheckCollisionRecs(
			(Rectangle) {
			model.player.position.x, model.player.position.y, model.player.size, model.player.size
		},
			(Rectangle) {
			model.gold[i].position.x, model.gold[i].position.y, model.gold[i].size, model.gold[i].size
		})) {

			model.gold[i].active = false;
			spawnParticles(model.particles, model.gold[i].position, 20, GOLD);
			model.goldCollected++;
		}
	}
	return model;
}

GameModel updatePlayerMovement(GameModel model, float deltaTime, int tileSize)
{
	Vector2 newPosition = model.player.position;
	model.player.isMoving = false;
	if (IsKeyDown(KEY_RIGHT)) {
		newPosition.x += model.player.speed * deltaTime;
		model.player.direction = (Vector2){ 1, 0 };
		model.player.isMoving = true;
	}
	if (IsKeyDown(KEY_LEFT)) {
		newPosition.x -= model.player.speed * deltaTime;
		model.player.direction = (Vector2){ -1, 0 };
		model.player.isMoving = true;
	}
	if (IsKeyDown(KEY_UP)) {
		newPosition.y -= model.player.speed * deltaTime;
		model.player.direction = (Vector2){ 0, -1 };
		model.player.isMoving = true;
	}
	if (IsKeyDown(KEY_DOWN)) {
		newPosition.y += model.player.speed * deltaTime;
		model.player.direction = (Vector2){ 0, 1 };
		model.player.isMoving = true;
	}

	// Define the player and map rectangles
	Rectangle playerRect = {
		model.player.position.x - 10,
		model.player.position.y - 10,
		model.player.size - 10,  // Assuming player size is defined
		model.player.size - 10  // Assuming player size is defined
	};

	// Track the original position to revert if collision occurs
	Vector2 originalPosition = model.player.position;

	// Move player to the new position
	model.player.position = newPosition;
	playerRect.x = model.player.position.x;
	playerRect.y = model.player.position.y;

	// Check collisions and adjust position if needed
	bool collisionDetected = false;
	for (int y = 0; y < MAP_HEIGHT; y++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			if (map[y][x] == '#') {
				Rectangle tileRect = {
					x * tileSize,
					y * tileSize,
					tileSize,
					tileSize
				};
				if (CheckCollisionRecs(playerRect, tileRect)) {
					collisionDetected = true;

					// Move player back to original position
					model.player.position = originalPosition;
					playerRect.x = model.player.position.x;
					playerRect.y = model.player.position.y;

					// Handle collision by checking and resolving separately for X and Y axes
					// Check horizontal collision
					if (CheckCollisionRecs(playerRect, tileRect)) {
						if (newPosition.x > originalPosition.x) {
							// Player is moving right
							model.player.position.x = tileRect.x - playerRect.width;
						}
						else {
							// Player is moving left
							model.player.position.x = tileRect.x + tileRect.width;
						}
					}

					// Update player rectangle position
					playerRect.x = model.player.position.x;
					playerRect.y = model.player.position.y;

					// Check vertical collision
					if (CheckCollisionRecs(playerRect, tileRect)) {
						if (newPosition.y > originalPosition.y) {
							// Player is moving down
							model.player.position.y = tileRect.y - playerRect.height;
						}
						else {
							// Player is moving up
							model.player.position.y = tileRect.y + tileRect.height;
						}
					}

					// Break out of loops once collision is resolved
					break;
				}
			}
		}
		if (collisionDetected) break;
	}

	return model;

}
// Calculate the movement step towards the next path node
void moveEnemyTowards(Vector2* enemyPos, Vector2 targetPos, float speed, float deltaTime) {
	// Calculate direction vector from enemy to the target position
	Vector2 direction = { targetPos.x - enemyPos->x, targetPos.y - enemyPos->y };

	// Calculate the distance to the target
	float distanceToTarget = sqrtf(direction.x * direction.x + direction.y * direction.y);

	if (distanceToTarget > 0) {
		// Normalize the direction vector
		direction.x /= distanceToTarget;
		direction.y /= distanceToTarget;

		// Calculate the step size based on speed and deltaTime
		float step = speed * deltaTime;

		// Move the enemy towards the target position
		if (step < distanceToTarget) {
			enemyPos->x += direction.x * step;
			enemyPos->y += direction.y * step;
		}
		else {
			// If the step is larger than the distance, just snap to the target
			enemyPos->x = targetPos.x;
			enemyPos->y = targetPos.y;
		}
	}
}


GameModel updateEnemies(GameModel model, float deltaTime, int tileSize)
{
	for (int i = 0; i < MAX_ENEMIES; i++) {
		if (model.enemies[i].active) {
			// Update attack and damage text timers
			if (model.enemies[i].attackCooldown > 0) {
				model.enemies[i].attackCooldown -= deltaTime;
			}
			if (model.enemies[i].damageTextTimer > 0) {
				model.enemies[i].damageTextTimer -= deltaTime;
			}

			// Pathfinding
			Node* path = findPath(model.enemies[i].position, model.player.position, tileSize);
			if (path != NULL) {
				Vector2 nextPosition = getNextPathPosition(path, &model.enemies[i], tileSize);

				// Move enemy towards the next path node using their speed and deltaTime
				Vector2 desiredPosition = model.enemies[i].position;
				moveEnemyTowards(&desiredPosition, nextPosition, model.enemies[i].speed, deltaTime);

				// Check map collisions and adjust position if necessary
				int enemyMapX = desiredPosition.x / tileSize;
				int enemyMapY = desiredPosition.y / tileSize;

				if (enemyMapY >= 0 && enemyMapY < MAP_HEIGHT && enemyMapX >= 0 && enemyMapX < MAP_WIDTH) {
					if (map[enemyMapY][enemyMapX] != '#') {
						Rectangle enemyRect = { desiredPosition.x, desiredPosition.y, model.enemies[i].size, model.enemies[i].size };
						Rectangle playerRect = { model.player.position.x, model.player.position.y, model.player.size, model.player.size };

						bool collisionWithPlayer = CheckCollisionRecs(enemyRect, playerRect);
						if (collisionWithPlayer && model.enemies[i].attackCooldown <= 0) {
							model.player.health--;
							spawnDamageParticle(&model,
								(Vector2) {
								model.player.position.x + model.player.size / 2, model.player.position.y
							}, 1, BLUE);
							model.enemies[i].attackCooldown = 1.0f;
							spawnParticles(model.particles, (Vector2) { model.player.position.x + model.player.size / 2, model.player.position.y + model.player.size / 2 }, 10, BLUE);
						}

						bool collisionWithOtherEnemy = false;
						for (int j = 0; j < MAX_ENEMIES; j++) {
							if (j != i && model.enemies[j].active) {
								Rectangle otherEnemyRect = { model.enemies[j].position.x, model.enemies[j].position.y, model.enemies[j].size, model.enemies[j].size };
								if (CheckCollisionRecs(enemyRect, otherEnemyRect)) {
									collisionWithOtherEnemy = true;
									break;
								}
							}
						}

						if (!collisionWithPlayer && !collisionWithOtherEnemy) {
							model.enemies[i].position = desiredPosition; // Update position if no collision
						}
						else {
							// Adjust position if there's a collision
							Vector2 tempPosition = model.enemies[i].position;

							// Try adjusting the position along X
							tempPosition.x = desiredPosition.x;
							enemyRect.x = tempPosition.x;

							bool collisionX = CheckCollisionRecs(enemyRect, playerRect);
							collisionWithOtherEnemy = false;
							for (int j = 0; j < MAX_ENEMIES; j++) {
								if (j != i && model.enemies[j].active) {
									Rectangle otherEnemyRect = { model.enemies[j].position.x, model.enemies[j].position.y, model.enemies[j].size, model.enemies[j].size };
									if (CheckCollisionRecs(enemyRect, otherEnemyRect)) {
										collisionWithOtherEnemy = true;
										break;
									}
								}
							}

							if (!collisionX && !collisionWithOtherEnemy) {
								model.enemies[i].position.x = tempPosition.x;
							}

							// Try adjusting the position along Y
							tempPosition = model.enemies[i].position;
							tempPosition.y = desiredPosition.y;
							enemyRect.y = tempPosition.y;

							bool collisionY = CheckCollisionRecs(enemyRect, playerRect);
							collisionWithOtherEnemy = false;
							for (int j = 0; j < MAX_ENEMIES; j++) {
								if (j != i && model.enemies[j].active) {
									Rectangle otherEnemyRect = { model.enemies[j].position.x, model.enemies[j].position.y, model.enemies[j].size, model.enemies[j].size };
									if (CheckCollisionRecs(enemyRect, otherEnemyRect)) {
										collisionWithOtherEnemy = true;
										break;
									}
								}
							}

							if (!collisionY && !collisionWithOtherEnemy) {
								model.enemies[i].position.y = tempPosition.y;
							}
						}
					}
				}
			}
		}
	}


	return model;
}
	

GameModel updateBullets(GameModel model, float deltaTime, int tileSize)
{
	if (IsKeyPressed(KEY_SPACE)) {
		for (int i = 0; i < MAX_BULLETS; i++) {
			if (!model.bullets[i].active) {
				model.bullets[i].position = (Vector2){ model.player.position.x + model.player.size / 2, model.player.position.y + model.player.size / 2 };
				model.bullets[i].direction = model.player.direction;
				model.bullets[i].active = true;
				break;
			}
		}
	}

	for (int i = 0; i < MAX_BULLETS; i++) {
		if (model.bullets[i].active) {
			model.bullets[i].position.x += model.bullets[i].direction.x * model.bullets[i].speed * deltaTime;
			model.bullets[i].position.y += model.bullets[i].direction.y * model.bullets[i].speed * deltaTime;

			if (model.bullets[i].position.x < 0 || model.bullets[i].position.x > MAP_WIDTH * tileSize ||
				model.bullets[i].position.y < 0 || model.bullets[i].position.y > MAP_HEIGHT * tileSize) {
				model.bullets[i].active = false;
			}

			for (int j = 0; j < MAX_ENEMIES; j++) {
				if (model.enemies[j].active && CheckCollisionRecs((Rectangle) { model.bullets[i].position.x, model.bullets[i].position.y, model.bullets[i].size, model.bullets[i].size },
					(Rectangle) {
					model.enemies[j].position.x, model.enemies[j].position.y, model.enemies[j].size, model.enemies[j].size
				})) {
					model.enemies[j].health--;
					model.bullets[i].active = false;
					spawnDamageParticle(&model,
						(Vector2) {
						model.enemies[j].position.x + model.enemies[j].size / 2, model.enemies[j].position.y
					}, 1, RED);
					spawnParticles(model.particles, (Vector2) { model.enemies[j].position.x + model.enemies[j].size / 2, model.enemies[j].position.y + model.enemies[j].size / 2 }, 5, RED);
					if (model.enemies[j].health <= 0) {
						model.enemies[j].active = false;
						model.killCount++;
					}
					break;
				}
			}
		}
	}
	return model;
}

GameModel updateDamagePartical(GameModel model, float deltaTime)
{
	for (int i = 0; i < MAX_DAMAGE_PARTICLES; i++) {
		if (model.damageParticles[i].active) {
			model.damageParticles[i].position.y += model.damageParticles[i].velocity.y * deltaTime;
			model.damageParticles[i].lifetime -= deltaTime;
			if (model.damageParticles[i].lifetime <= 0.0f) {
				model.damageParticles[i].active = false;
			}
		}
	}
	return model;
}

GameModel updateSword(GameModel model, float deltaTime, int tileSize)
{
	if (model.sword.cooldown > 0.0f) {
		model.sword.cooldown -= deltaTime;
	}

	if (model.sword.active) {
		model.sword.duration -= deltaTime;
		if (model.sword.duration <= 0.0f) {
			model.sword.active = false;
		}
	}

	// Sword attack logic
	if (IsKeyDown(KEY_C) && model.sword.cooldown <= 0.0f) {
		model.sword.active = true;
		model.sword.cooldown = SWORD_COOLDOWN;
		model.sword.duration = SWORD_DURATION;

		// Adjust sword orientation based on player direction
		float swordOffset = model.player.size / 3; // Make the sword closer by reducing the offset

		if (model.player.direction.y != 0) { // Moving up or down (vertical sword)
			model.sword.size = (Vector2){ SWORD_WIDTH, SWORD_HEIGHT };
			model.sword.position = (Vector2){
				model.player.position.x + model.player.size / 2 - model.sword.size.x / 2,
				model.player.position.y + model.player.size / 2 + model.player.direction.y * (model.player.size - swordOffset)
			};
		}
		else if (model.player.direction.x != 0) { // Moving left or right (horizontal sword)
			model.sword.size = (Vector2){ SWORD_HEIGHT, SWORD_WIDTH };
			model.sword.position = (Vector2){
				model.player.position.x + model.player.size / 2 + model.player.direction.x * (model.player.size - swordOffset),
				model.player.position.y + model.player.size / 2 - model.sword.size.y / 2
			};
		}

		// Sword hitbox (centered on the position)
		Rectangle swordRect = {
			model.sword.position.x - model.sword.size.x / 2,
			model.sword.position.y - model.sword.size.y / 2,
			model.sword.size.x, model.sword.size.y
		};

		// Check for collisions with enemies
		for (int i = 0; i < MAX_ENEMIES; i++) {
			if (model.enemies[i].active && CheckCollisionRecs(swordRect,
				(Rectangle) {
				model.enemies[i].position.x, model.enemies[i].position.y, model.enemies[i].size, model.enemies[i].size
			})) {
				spawnDamageParticle(&model,
					(Vector2) {
					model.enemies[i].position.x + model.enemies[i].size / 2, model.enemies[i].position.y
				}, 1, RED);

				model.enemies[i].health--;
				spawnParticles(model.particles,
					(Vector2) {
					model.enemies[i].position.x + model.enemies[i].size / 2, model.enemies[i].position.y + model.enemies[i].size / 2
				}, 10, RED);

				if (model.enemies[i].health <= 0) {
					model.enemies[i].active = false;
					model.killCount++;
				}
			}
		}
	}

	return model;
}


GameModel updateNpcs(GameModel model, float deltaTime, int tileSize)
{

}


GameModel update(GameModel model, float deltaTime, int tileSize)
{
	model = updatePlayerMovement(model, deltaTime, tileSize);
	model = updateEnemies(model, deltaTime, tileSize);
	model = updateBullets(model, deltaTime, tileSize);
	model = updateSword(model, deltaTime, tileSize);
	model = updateCrates(model, tileSize);
	model = updateGold(model, deltaTime);
	updateParticles(model.particles, deltaTime);
	model = updateDamagePartical(model, deltaTime);
	model = updateStage(model, deltaTime, tileSize);
	model.activeDialog = NULL;
	for (int i = 0; i < MAX_NPCS; i++) {
		if (model.npcs[i].active &&
			CheckCollisionRecs((Rectangle) { model.player.position.x, model.player.position.y, model.player.size, model.player.size },
				(Rectangle) {
			model.npcs[i].position.x, model.npcs[i].position.y, model.npcs[i].size, model.npcs[i].size
		})) {
			model.activeDialog = model.npcs[i].dialog;
			if (IsKeyPressed(KEY_E)) {
				model.npcs[i].interact = true;
			}
			break;
		}
	}
	return model;
}

#pragma endregion

#pragma region Draw


void drawASCII(Vector2 position, const char* grid[8], int scale, Color color) {
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			if (grid[y][x] == 'X') {
				DrawRectangle(position.x + x * scale, position.y + y * scale, scale, scale, color);
			}
		}
	}
}

void drawParticles(Particle particles[]) {
	for (int i = 0; i < MAX_PARTICLES; i++) {
		if (particles[i].active) {
			// Draw as small circles or any shape you prefer
			DrawCircleV(particles[i].position, 5, particles[i].color);
		}
	}
}

void drawCrates(Crate crates[]) {
	for (int i = 0; i < MAX_CRATES; i++) {
		if (crates[i].active) {
			DrawRectangle(crates[i].position.x, crates[i].position.y, crates[i].size, crates[i].size, crates[i].color);
		}
	}
}

void drawGold(Gold gold[]) {
	for (int i = 0; i < MAX_GOLD; i++) {
		if (gold[i].active) {
			DrawCircleV(gold[i].position, gold[i].size / 2, YELLOW);
		}
	}
}

GameModel draw(GameModel model, float deltaTime) {
	//DrawRectangle(model.player.position.x, model.player.position.y, model.player.size, model.player.size, model.player.color);
	animationTimer += deltaTime;
	if (animationTimer >= frameDuration) {
		animationFrame = (animationFrame + 1) % 2;  // Toggle between 0 and 1 for animation
		animationTimer = 0.0f;
	}
	drawCrates(model.crates);
	// Draw player based on the current state
	//DrawRectangle(model.player.position.x, model.player.position.y, model.player.size, model.player.size, GRAY);
	if (model.player.isMoving) {
		drawASCII(model.player.position, playerWalking[animationFrame], 8, BLUE);
	}
	else {
		drawASCII(model.player.position, playerIdle[animationFrame], 8, BLUE);
	}


	drawGold(model.gold);
	
	for (int i = 0; i < MAX_ENEMIES; i++) {
		if (model.enemies[i].active) {

			float healthBarWidth = model.enemies[i].size;
			float healthBarHeight = 5.0f; // Height of the health bar
			float healthPercentage = (float)model.enemies[i].health / 3; // Assuming max health is 10

			DrawRectangle(model.enemies[i].position.x, model.enemies[i].position.y - healthBarHeight - 2, healthBarWidth, healthBarHeight, DARKGRAY);
			DrawRectangle(model.enemies[i].position.x, model.enemies[i].position.y - healthBarHeight - 2, healthBarWidth * healthPercentage, healthBarHeight, RED);
			if (model.enemies[i].damageTextTimer > 0) {
				char damageText[16];
				sprintf(damageText, "-%d", 1);
				Vector2 damagePosition = {
					model.enemies[i].position.x + model.enemies[i].size / 2,
					model.enemies[i].position.y - healthBarHeight - 10
				};
				DrawText(damageText, damagePosition.x, damagePosition.y, 30, BLACK);
			}
			//DrawRectangle(model.enemies[i].position.x, model.enemies[i].position.y, model.enemies[i].size, model.enemies[i].size, model.enemies[i].color);
			drawASCII(model.enemies[i].position, enemy[animationFrame], 8, RED);
		}
	}
	if (model.sword.active) {
		DrawRectangle(model.sword.position.x - model.sword.size.x / 2
			, model.sword.position.y - model.sword.size.y / 2
			, model.sword.size.x
			, model.sword.size.y
			, model.sword.color
		);
	}
	for (int i = 0; i < MAX_BULLETS; i++) {
		if (model.bullets[i].active) {
			DrawRectangle(model.bullets[i].position.x, model.bullets[i].position.y, model.bullets[i].size, model.bullets[i].size, model.bullets[i].color);
		}
	}
	drawParticles(model.particles);
	for (int i = 0; i < MAX_NPCS; i++) {
		if (model.npcs[i].active) {
			drawASCII(model.npcs[i].position, npc[animationFrame], 8, GREEN);
			//DrawRectangle(model.npcs[i].position.x, model.npcs[i].position.y, model.npcs[i].size, model.npcs[i].size, model.npcs[i].color);
		}
	}

	for (int i = 0; i < MAX_DAMAGE_PARTICLES; i++) {
		if (model.damageParticles[i].active) {
			char damageText[16];
			sprintf(damageText, "-%d", model.damageParticles[i].damageAmount);
			DrawText(damageText, model.damageParticles[i].position.x, model.damageParticles[i].position.y, 20, model.damageParticles[i].color);
		}
	}

	return model;
}
#pragma endregion


int main(void)
{
	const int screenWidth = 800;
	const int screenHeight = 450;
	const int tileSize = 50;

	InitWindow(screenWidth, screenHeight, "Barp");

	GameModel model = setup(tileSize);

	Camera2D camera = { 0 };
	camera.target = (Vector2){ model.player.position.x + model.player.size / 2, model.player.position.y + model.player.size / 2 };
	camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
	camera.rotation = 0.0f;
	camera.zoom = 1.0f;

	SetTargetFPS(60);
	while (!WindowShouldClose())
	{
		float deltaTime = GetFrameTime();
		camera.target = (Vector2){ model.player.position.x + model.player.size / 2, model.player.position.y + model.player.size / 2 };
		model = update(model, deltaTime, tileSize);
		BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode2D(camera);
		// map
		for (int y = 0; y < MAP_HEIGHT; y++) {
			for (int x = 0; x < MAP_WIDTH; x++) {
				if (map[y][x] == '#') {
					DrawRectangle(x * tileSize, y * tileSize, tileSize, tileSize, GRAY);
				}
			}
		}
		draw(model, deltaTime);
		EndMode2D();
		if (model.activeDialog != NULL) {
			DrawRectangle(50, screenHeight - 100, screenWidth - 100, 50, Fade(LIGHTGRAY, 0.8f));
			DrawText(model.activeDialog, 60, screenHeight - 90, 20, BLACK);
		}
		DrawText(TextFormat("Health: %d", model.player.health), 10, 10, 20, BLACK);
		DrawText(TextFormat("Gold: %d", model.goldCollected), 10, 30, 20, BLACK);
		EndDrawing();
	}

	CloseWindow();

	return 0;
}

