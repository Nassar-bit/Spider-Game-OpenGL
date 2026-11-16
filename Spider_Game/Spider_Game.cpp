// ============================================================================
//  SPIDER GAME  -  Sprite Animation and Collision in OpenGL
//  CT-384: Graphics Design  |  Complex Computing Activity (CCA)
//
//  Build (Windows, Code::Blocks / Dev-C++ with freeglut):
//      g++ SpiderGame.cpp -o SpiderGame -lfreeglut -lopengl32 -lglu32
//  Build (Linux):   g++ SpiderGame.cpp -o SpiderGame -lglut -lGL -lGLU
//  Build (macOS):   g++ SpiderGame.cpp -o SpiderGame -framework GLUT -framework OpenGL
//
//  STORY: You are a spider guarding your web. Shoot web-balls at the flies
//  (green, +10) and the wasps (yellow, +20). Every bug can fly all the way down
//  to the ground, and any bug that touches the spider costs you a life.
//
//  CONTROLS
//      Arrow keys / W A S D ... move the spider
//      Mouse move ............. aim
//      Left click / SPACE ..... shoot web-ball
//      1 / 2 / 3 .............. difficulty (Easy / Normal / Hard) - restarts game
//      P ...................... pause        R ... restart        ESC ... quit
//
//  CCA REQUIREMENTS COVERED
//   [x] Sprite motion using velocity  dx = s*cos(theta)*dt ,  dy = s*sin(theta)*dt
//   [x] glutTimerFunc() animation loop (~60 FPS) through move(int t)
//   [x] Boundary bounce (direction reverses at the screen edges)
//   [x] Distance based circle collision  (dist < r1 + r2)
//   [x] Projectiles with their own position/velocity, drawBullet()
//   Ideas to work:
//   [x] 1 Keyboard controls for the player sprite
//   [x] 2 Multiple enemies, random speeds and angles
//   [x] 3 Scoring system (bullet hits)
//   [x] 4 Sound effects (hit, sting, level up, game over)
//   [x] 5 Difficulty levels (3 modes + level increases as you score)
//   [x] 6 Extra: lives, invulnerability blink, particle explosions, night
//          background with twinkling stars, house, web corner, pause, restart,
//          game-over screen, high score
// ============================================================================

#include "stdafx.h"               // precompiled header (Visual Studio project) - must be FIRST

#ifdef _MSC_VER
#pragma warning(disable : 4996)   // lets sprintf compile in Visual Studio
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

const float PI = 3.14159265f;

// ----------------------------------------------------------------------------
//  Data types
// ----------------------------------------------------------------------------
struct Enemy {
	float x, y;      // position
	float r;         // collision radius
	float s;         // speed (units / second)
	float theta;     // direction (radians)
	bool  wasp;      // true = wasp (dangerous), false = fly
	float phase;     // wing flap offset
};

struct Bullet {
	float x, y;      // position
	float dx, dy;    // velocity per second
};

struct Particle {
	float x, y, dx, dy, life;
	float r, g, b;
};

// ----------------------------------------------------------------------------
//  Global state
// ----------------------------------------------------------------------------
std::vector<Enemy>    enemies;
std::vector<Bullet>   bullets;
std::vector<Particle> particles;

// Player (spider) - the "posx, posy, dx, dy" style globals from the skeleton
float posx = 0.0f, posy = -0.7f;       // spider position
float dx = 0.0f, dy = 0.0f;           // spider velocity this frame
const float spiderR = 0.07f;       // spider collision radius
const float spiderSpeed = 0.9f;        // units per second
const float bulletR = 0.02f;       // bullet radius
const float bulletSpeed = 1.7f;

int   score = 0, highScore = 0, lives = 3, level = 1;
int   difficulty = 2;                  // 1 easy, 2 normal, 3 hard
float diffMul = 1.0f;
bool  paused = false, gameOver = false;
float invuln = 0.0f;                   // seconds of invulnerability after a sting
float cooldown = 0.0f;                 // shoot cooldown
float gameTime = 0.0f;
float mouseX = 0.0f, mouseY = 1.0f;    // aim point
bool  keys[256];
bool  specialKeys[4];                  // up, down, left, right
float starX[60], starY[60], starP[60]; // background stars

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
float rnd(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

void playSound(int type) {
#ifdef _WIN32
	// MessageBeep is asynchronous, so the game never freezes
	if (type == 0) MessageBeep(MB_OK);            // hit
	else if (type == 1) MessageBeep(MB_ICONHAND);      // sting
	else if (type == 2) MessageBeep(MB_ICONASTERISK);  // level up
	else                MessageBeep(MB_ICONEXCLAMATION); // game over
#else
	(void)type;
	printf("\a");
	fflush(stdout);
#endif
}

void drawText(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) {
	glRasterPos2f(x, y);
	for (; *s; ++s) glutBitmapCharacter(font, *s);
}

void drawCircle(float cx, float cy, float r, int seg = 40) {
	glBegin(GL_POLYGON);
	for (int i = 0; i < seg; i++) {
		float a = 2.0f * PI * i / seg;
		glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
	}
	glEnd();
}

void drawEllipse(float cx, float cy, float rx, float ry, float rot = 0.0f, int seg = 30) {
	glBegin(GL_POLYGON);
	for (int i = 0; i < seg; i++) {
		float a = 2.0f * PI * i / seg;
		float px = rx * cosf(a), py = ry * sinf(a);
		glVertex2f(cx + px * cosf(rot) - py * sinf(rot),
			cy + px * sinf(rot) + py * cosf(rot));
	}
	glEnd();
}

// ----------------------------------------------------------------------------
//  Sprites
// ----------------------------------------------------------------------------

// Bullet sprite (web ball) - same idea as drawBullet() in the handout
void drawBullet(float bx, float by) {
	glColor4f(0.8f, 0.95f, 1.0f, 0.35f);          // soft glow
	drawCircle(bx, by, bulletR * 1.8f, 20);
	glColor3f(1.0f, 1.0f, 1.0f);                  // core
	glBegin(GL_POLYGON);
	for (int i = 0; i < 360; i += 10) {
		float angle = i * 3.14159f / 180.0f;
		glVertex2f(bx + bulletR * cosf(angle), by + bulletR * sinf(angle));
	}
	glEnd();
}

// House sprite (background decoration) - drawHouse() from the handout
void drawHouse(float x, float y, float s) {
	// walls
	glColor3f(0.45f, 0.30f, 0.20f);
	glBegin(GL_QUADS);
	glVertex2f(x, y);           glVertex2f(x + 0.30f * s, y);
	glVertex2f(x + 0.30f * s, y + 0.20f * s); glVertex2f(x, y + 0.20f * s);
	glEnd();
	// roof
	glColor3f(0.55f, 0.12f, 0.12f);
	glBegin(GL_TRIANGLES);
	glVertex2f(x - 0.03f * s, y + 0.20f * s);
	glVertex2f(x + 0.33f * s, y + 0.20f * s);
	glVertex2f(x + 0.15f * s, y + 0.36f * s);
	glEnd();
	// door
	glColor3f(0.22f, 0.12f, 0.07f);
	glBegin(GL_QUADS);
	glVertex2f(x + 0.04f * s, y);        glVertex2f(x + 0.11f * s, y);
	glVertex2f(x + 0.11f * s, y + 0.12f * s); glVertex2f(x + 0.04f * s, y + 0.12f * s);
	glEnd();
	// lit window (flickers a little)
	float glow = 0.85f + 0.15f * sinf(gameTime * 3.0f);
	glColor3f(1.0f * glow, 0.9f * glow, 0.4f * glow);
	glBegin(GL_QUADS);
	glVertex2f(x + 0.17f * s, y + 0.07f * s); glVertex2f(x + 0.26f * s, y + 0.07f * s);
	glVertex2f(x + 0.26f * s, y + 0.15f * s); glVertex2f(x + 0.17f * s, y + 0.15f * s);
	glEnd();
	glColor3f(0.2f, 0.1f, 0.05f);
	glLineWidth(2);
	glBegin(GL_LINES);
	glVertex2f(x + 0.215f * s, y + 0.07f * s); glVertex2f(x + 0.215f * s, y + 0.15f * s);
	glVertex2f(x + 0.17f * s, y + 0.11f * s);  glVertex2f(x + 0.26f * s, y + 0.11f * s);
	glEnd();
}

void drawSpider(float x, float y) {
	float wig = sinf(gameTime * 14.0f) * 0.012f;     // leg animation

	// legs (4 per side, two segments each)
	glColor3f(0.05f, 0.05f, 0.05f);
	glLineWidth(3);
	for (int side = -1; side <= 1; side += 2) {
		for (int i = 0; i < 4; i++) {
			float baseA = (0.25f + i * 0.28f) * (float)side;         // fan out
			float lift = (i % 2 == 0 ? wig : -wig);
			float kx = x + side * 0.07f, ky = y + (0.045f - i * 0.03f);
			float mx = kx + side * 0.05f, my = ky + 0.045f + lift;
			float ex = mx + side * 0.05f * cosf(baseA * 0.5f);
			float ey = my - 0.085f + lift - i * 0.005f;
			glBegin(GL_LINE_STRIP);
			glVertex2f(x + side * 0.025f, y + (0.02f - i * 0.012f));
			glVertex2f(mx, my);
			glVertex2f(ex, ey);
			glEnd();
		}
	}
	// abdomen + head
	glColor3f(0.12f, 0.05f, 0.18f);
	drawEllipse(x, y - 0.03f, 0.055f, 0.065f);
	glColor3f(0.75f, 0.1f, 0.1f);                   // red hourglass mark
	glBegin(GL_TRIANGLES);
	glVertex2f(x - 0.015f, y - 0.01f); glVertex2f(x + 0.015f, y - 0.01f); glVertex2f(x, y - 0.035f);
	glVertex2f(x - 0.015f, y - 0.06f); glVertex2f(x + 0.015f, y - 0.06f); glVertex2f(x, y - 0.035f);
	glEnd();
	glColor3f(0.10f, 0.04f, 0.14f);
	drawCircle(x, y + 0.04f, 0.035f);
	// eyes follow the aim direction
	float ang = atan2f(mouseY - y, mouseX - x);
	glColor3f(1, 1, 1);
	drawCircle(x - 0.014f, y + 0.05f, 0.011f, 16);
	drawCircle(x + 0.014f, y + 0.05f, 0.011f, 16);
	glColor3f(0, 0, 0);
	drawCircle(x - 0.014f + 0.005f * cosf(ang), y + 0.05f + 0.005f * sinf(ang), 0.005f, 12);
	drawCircle(x + 0.014f + 0.005f * cosf(ang), y + 0.05f + 0.005f * sinf(ang), 0.005f, 12);
}

void drawEnemy(const Enemy& e) {
	float flap = sinf(gameTime * 30.0f + e.phase);
	float face = (cosf(e.theta) >= 0) ? 1.0f : -1.0f;

	// wings
	glColor4f(0.7f, 0.85f, 1.0f, 0.55f);
	drawEllipse(e.x - 0.01f * face, e.y + 0.035f, 0.015f, 0.04f + 0.015f * flap, 0.4f);
	drawEllipse(e.x + 0.01f * face, e.y + 0.035f, 0.015f, 0.04f - 0.015f * flap, -0.4f);

	if (e.wasp) {
		glColor3f(0.95f, 0.8f, 0.1f);                       // yellow body
		drawEllipse(e.x, e.y, e.r * 0.8f, e.r, 0.0f);
		glColor3f(0.05f, 0.05f, 0.05f);                     // black stripes
		for (int i = -1; i <= 1; i++) {
			glBegin(GL_QUADS);
			float yy = e.y + i * e.r * 0.45f;
			glVertex2f(e.x - e.r * 0.7f, yy - 0.008f); glVertex2f(e.x + e.r * 0.7f, yy - 0.008f);
			glVertex2f(e.x + e.r * 0.7f, yy + 0.008f); glVertex2f(e.x - e.r * 0.7f, yy + 0.008f);
			glEnd();
		}
		glColor3f(0.9f, 0.1f, 0.1f);                        // angry eyes
		drawCircle(e.x - 0.015f, e.y + e.r * 0.7f, 0.009f, 12);
		drawCircle(e.x + 0.015f, e.y + e.r * 0.7f, 0.009f, 12);
	}
	else {
		glColor3f(0.25f, 0.65f, 0.25f);                     // green fly
		drawEllipse(e.x, e.y, e.r * 0.75f, e.r * 0.9f, 0.0f);
		glColor3f(0.1f, 0.3f, 0.1f);
		drawCircle(e.x, e.y + e.r * 0.8f, e.r * 0.45f, 20);
		glColor3f(1, 0.2f, 0.2f);                           // red eyes
		drawCircle(e.x - 0.012f, e.y + e.r * 0.9f, 0.008f, 12);
		drawCircle(e.x + 0.012f, e.y + e.r * 0.9f, 0.008f, 12);
	}
}

void drawBackground() {
	// night sky gradient
	glBegin(GL_QUADS);
	glColor3f(0.02f, 0.02f, 0.10f); glVertex2f(-1, 1);  glVertex2f(1, 1);
	glColor3f(0.12f, 0.08f, 0.25f); glVertex2f(1, -1);  glVertex2f(-1, -1);
	glEnd();

	// twinkling stars
	glPointSize(2.5f);
	glBegin(GL_POINTS);
	for (int i = 0; i < 60; i++) {
		float b = 0.5f + 0.5f * sinf(gameTime * 2.0f + starP[i]);
		glColor4f(1, 1, 0.9f, 0.3f + 0.7f * b);
		glVertex2f(starX[i], starY[i]);
	}
	glEnd();

	// moon
	glColor4f(1.0f, 1.0f, 0.85f, 0.15f); drawCircle(0.72f, 0.78f, 0.17f);
	glColor3f(0.97f, 0.97f, 0.85f);       drawCircle(0.72f, 0.78f, 0.11f);
	glColor3f(0.85f, 0.85f, 0.72f);       drawCircle(0.69f, 0.80f, 0.02f, 16);
	drawCircle(0.75f, 0.75f, 0.015f, 16);

	// ground
	glColor3f(0.05f, 0.18f, 0.08f);
	glBegin(GL_QUADS);
	glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, -0.88f); glVertex2f(-1, -0.88f);
	glEnd();

	drawHouse(-0.92f, -0.90f, 1.0f);
	drawHouse(0.62f, -0.90f, 0.8f);

	// web in the top-left corner
	glColor4f(1, 1, 1, 0.25f);
	glLineWidth(1);
	glBegin(GL_LINES);
	for (int i = 0; i < 6; i++) {
		float a = -PI / 2 + i * (PI / 2) / 5.0f;
		glVertex2f(-1, 1); glVertex2f(-1 + 0.5f * cosf(a), 1 + 0.5f * sinf(a));
	}
	glEnd();
	for (int k = 1; k <= 4; k++) {
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 6; i++) {
			float a = -PI / 2 + i * (PI / 2) / 5.0f;
			glVertex2f(-1 + 0.12f * k * cosf(a), 1 + 0.12f * k * sinf(a));
		}
		glEnd();
	}
}

void drawHUD() {
	char buf[100];
	glColor3f(1, 1, 1);
	sprintf(buf, "Score: %d", score);          drawText(-0.95f, 0.92f, buf);
	sprintf(buf, "Level: %d", level);          drawText(-0.95f, 0.85f, buf);
	sprintf(buf, "Best: %d", highScore);       drawText(-0.95f, 0.78f, buf, GLUT_BITMAP_HELVETICA_12);

	const char* dn = (difficulty == 1) ? "EASY" : (difficulty == 2) ? "NORMAL" : "HARD";
	sprintf(buf, "Mode: %s", dn);             drawText(-0.18f, 0.92f, buf, GLUT_BITMAP_HELVETICA_12);

	// lives as little hearts (circles)
	drawText(0.28f, 0.92f, "Lives:", GLUT_BITMAP_HELVETICA_18);
	for (int i = 0; i < lives; i++) {
		glColor3f(0.95f, 0.15f, 0.25f);
		drawCircle(0.52f + i * 0.055f, 0.925f, 0.02f, 16);
	}
	glColor3f(0.8f, 0.8f, 0.8f);
	drawText(-0.95f, -0.97f, "Arrows/WASD move | Mouse aim | Click/SPACE shoot | 1-2-3 mode | P pause | R restart",
		GLUT_BITMAP_HELVETICA_10);
}

// ----------------------------------------------------------------------------
//  Game logic
// ----------------------------------------------------------------------------
void spawnEnemy(bool wasp) {
	Enemy e;
	e.wasp = wasp;
	e.r = wasp ? 0.055f : 0.05f;
	e.x = rnd(-0.8f, 0.8f);
	e.y = rnd(0.35f, 0.85f);
	e.s = rnd(0.25f, 0.55f) * (1.0f + 0.10f * (level - 1)) * diffMul;
	if (wasp) e.s *= 1.25f;
	e.theta = rnd(0.0f, 2.0f * PI);                    // random angle
	e.phase = rnd(0.0f, 6.28f);
	enemies.push_back(e);
}

void refillEnemies() {
	int wantFlies = 3 + level / 2 + (difficulty - 1);
	int wantWasps = (level >= 2 ? (level - 1) / 2 + 1 : 0) + (difficulty == 3 ? 1 : 0);
	if (wantFlies > 8) wantFlies = 8;
	if (wantWasps > 5) wantWasps = 5;
	int flies = 0, wasps = 0;
	for (size_t i = 0; i < enemies.size(); i++) (enemies[i].wasp ? wasps : flies)++;
	while (flies < wantFlies) { spawnEnemy(false); flies++; }
	while (wasps < wantWasps) { spawnEnemy(true);  wasps++; }
}

void explode(float x, float y, float r, float g, float b) {
	for (int i = 0; i < 18; i++) {
		Particle p;
		float a = rnd(0, 2 * PI), sp = rnd(0.2f, 0.8f);
		p.x = x; p.y = y; p.dx = sp * cosf(a); p.dy = sp * sinf(a);
		p.life = rnd(0.3f, 0.7f); p.r = r; p.g = g; p.b = b;
		particles.push_back(p);
	}
}

void resetGame() {
	enemies.clear(); bullets.clear(); particles.clear();
	diffMul = (difficulty == 1) ? 0.8f : (difficulty == 2) ? 1.0f : 1.35f;
	lives = (difficulty == 1) ? 5 : (difficulty == 2) ? 3 : 2;
	score = 0; level = 1; invuln = 0; cooldown = 0;
	posx = 0.0f; posy = -0.7f;
	paused = false; gameOver = false;
	refillEnemies();
}

void shoot() {
	if (cooldown > 0 || paused || gameOver) return;
	float ang = atan2f(mouseY - posy, mouseX - posx); // full 360 degree aiming
	Bullet b;
	b.x = posx + 0.08f * cosf(ang);                   // start just outside the spider
	b.y = posy + 0.08f * sinf(ang);
	b.dx = bulletSpeed * cosf(ang);                   // velocity from angle + speed
	b.dy = bulletSpeed * sinf(ang);
	bullets.push_back(b);
	cooldown = 0.18f;
}

// Timer callback - called every 16 ms (~60 FPS)
void move(int t) {
	const float dt = 0.016f;                          // elapsed time per frame (s)

	if (!paused && !gameOver) {
		gameTime += dt;
		if (cooldown > 0) cooldown -= dt;
		if (invuln   > 0) invuln -= dt;

		// ----- player movement -----
		float vx = 0, vy = 0;
		if (specialKeys[2] || keys['a'] || keys['A']) vx -= 1;
		if (specialKeys[3] || keys['d'] || keys['D']) vx += 1;
		if (specialKeys[0] || keys['w'] || keys['W']) vy += 1;
		if (specialKeys[1] || keys['s'] || keys['S']) vy -= 1;
		if (vx != 0 && vy != 0) { vx *= 0.7071f; vy *= 0.7071f; }
		dx = spiderSpeed * vx * dt;
		dy = spiderSpeed * vy * dt;
		posx += dx;
		posy += dy;
		if (posx >  1 - spiderR) posx = 1 - spiderR;
		if (posx < -1 + spiderR) posx = -1 + spiderR;
		if (posy > 0.75f)        posy = 0.75f;      // spider can climb high up
		if (posy < -0.85f)       posy = -0.85f;
		if (keys[' ']) shoot();                       // hold space to keep shooting

		// ----- bullets -----
		for (size_t i = 0; i < bullets.size();) {
			bullets[i].x += bullets[i].dx * dt;
			bullets[i].y += bullets[i].dy * dt;
			if (bullets[i].x < -1.05f || bullets[i].x > 1.05f || bullets[i].y > 1.05f || bullets[i].y < -1.05f)
				bullets.erase(bullets.begin() + i);
			else i++;
		}

		// ----- enemies: velocity from speed + angle, bounce on walls -----
		for (size_t i = 0; i < enemies.size(); i++) {
			Enemy& e = enemies[i];
			float edx = e.s * cosf(e.theta) * dt;
			float edy = e.s * sinf(e.theta) * dt;
			e.x += edx;
			e.y += edy;

			float left = -1 + e.r, right = 1 - e.r, top = 1 - e.r;
			float bottom = -0.88f + e.r;              // bounce off the ground
			if (e.x > right) { e.x = right; e.theta = PI - e.theta; }   // dx = -dx
			if (e.x < left)  { e.x = left;  e.theta = PI - e.theta; }
			if (e.y > top)   { e.y = top;   e.theta = -e.theta; }       // dy = -dy
			if (e.y < bottom){ e.y = bottom; e.theta = -e.theta; }
			// small random wobble so paths are less predictable
			e.theta += rnd(-0.03f, 0.03f);
		}

		// ----- bullet vs enemy collision (circle distance) -----
		bool levelUp = false;
		for (size_t b = 0; b < bullets.size();) {
			bool hit = false;
			for (size_t i = 0; i < enemies.size(); i++) {
				float dist = sqrtf((bullets[b].x - enemies[i].x) * (bullets[b].x - enemies[i].x) +
					(bullets[b].y - enemies[i].y) * (bullets[b].y - enemies[i].y));
				if (dist < bulletR + enemies[i].r) {
					score += enemies[i].wasp ? 20 : 10;
					if (enemies[i].wasp) explode(enemies[i].x, enemies[i].y, 1.0f, 0.8f, 0.1f);
					else                 explode(enemies[i].x, enemies[i].y, 0.4f, 1.0f, 0.4f);
					enemies.erase(enemies.begin() + i);
					hit = true;
					playSound(0);
					break;
				}
			}
			if (hit) bullets.erase(bullets.begin() + b); else b++;
		}
		int newLevel = 1 + score / 100;
		if (newLevel > 10) newLevel = 10;
		if (newLevel > level) { level = newLevel; levelUp = true; }
		if (score > highScore) highScore = score;
		if (levelUp) playSound(2);
		refillEnemies();

		// ----- wasp vs spider collision -----
		if (invuln <= 0) {
			for (size_t i = 0; i < enemies.size(); i++) {
				float dist = sqrtf((posx - enemies[i].x) * (posx - enemies[i].x) +
					(posy - enemies[i].y) * (posy - enemies[i].y));
				if (dist < spiderR * 0.8f + enemies[i].r) {
					lives--;
					invuln = 1.5f;
					explode(posx, posy, 1.0f, 0.2f, 0.2f);
					enemies.erase(enemies.begin() + i);
					playSound(1);
					if (lives <= 0) { gameOver = true; playSound(3); }
					refillEnemies();
					break;
				}
			}
		}

		// ----- particles -----
		for (size_t i = 0; i < particles.size();) {
			particles[i].x += particles[i].dx * dt;
			particles[i].y += particles[i].dy * dt;
			particles[i].life -= dt;
			if (particles[i].life <= 0) particles.erase(particles.begin() + i); else i++;
		}
	}

	glutPostRedisplay();               // refresh the screen
	glutTimerFunc(16, move, 0);        // ~60 FPS
}

// ----------------------------------------------------------------------------
//  Rendering
// ----------------------------------------------------------------------------
void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	drawBackground();

	for (size_t i = 0; i < enemies.size(); i++) drawEnemy(enemies[i]);
	for (size_t i = 0; i < bullets.size(); i++) drawBullet(bullets[i].x, bullets[i].y);

	for (size_t i = 0; i < particles.size(); i++) {
		glColor4f(particles[i].r, particles[i].g, particles[i].b, particles[i].life * 1.5f);
		drawCircle(particles[i].x, particles[i].y, 0.012f, 10);
	}

	if (!gameOver && !(invuln > 0 && fmodf(invuln * 10.0f, 2.0f) < 1.0f)) {
		// web thread hanging from the top (spider silk)
		glColor4f(1, 1, 1, 0.3f);
		glLineWidth(1);
		glBegin(GL_LINES); glVertex2f(posx, 1.0f); glVertex2f(posx, posy + 0.08f); glEnd();
		drawSpider(posx, posy);
	}

	// crosshair
	if (!gameOver) {
		glColor4f(1, 1, 1, 0.8f);
		glLineWidth(1);
		glBegin(GL_LINES);
		glVertex2f(mouseX - 0.025f, mouseY); glVertex2f(mouseX + 0.025f, mouseY);
		glVertex2f(mouseX, mouseY - 0.025f); glVertex2f(mouseX, mouseY + 0.025f);
		glEnd();
	}

	drawHUD();

	if (paused) {
		glColor3f(1, 1, 0.3f);
		drawText(-0.13f, 0.05f, "PAUSED", GLUT_BITMAP_TIMES_ROMAN_24);
		glColor3f(1, 1, 1);
		drawText(-0.2f, -0.05f, "Press P to continue", GLUT_BITMAP_HELVETICA_12);
	}
	if (gameOver) {
		glColor4f(0, 0, 0, 0.6f);
		glBegin(GL_QUADS);
		glVertex2f(-0.6f, -0.3f); glVertex2f(0.6f, -0.3f); glVertex2f(0.6f, 0.35f); glVertex2f(-0.6f, 0.35f);
		glEnd();
		char buf[80];
		glColor3f(1, 0.25f, 0.25f);
		drawText(-0.22f, 0.22f, "GAME OVER", GLUT_BITMAP_TIMES_ROMAN_24);
		glColor3f(1, 1, 1);
		sprintf(buf, "Final score: %d   (Level %d)", score, level);
		drawText(-0.32f, 0.08f, buf);
		sprintf(buf, "High score: %d", highScore);
		drawText(-0.17f, -0.02f, buf, GLUT_BITMAP_HELVETICA_12);
		drawText(-0.27f, -0.15f, "Press R to play again", GLUT_BITMAP_HELVETICA_12);
	}

	glutSwapBuffers();
}

// ----------------------------------------------------------------------------
//  Input
// ----------------------------------------------------------------------------
void keyDown(unsigned char k, int, int) {
	keys[k] = true;
	if (k == 27) exit(0);
	if (k == 'p' || k == 'P') { if (!gameOver) paused = !paused; }
	if (k == 'r' || k == 'R') resetGame();
	if (k == '1' || k == '2' || k == '3') { difficulty = k - '0'; resetGame(); }
}
void keyUp(unsigned char k, int, int) { keys[k] = false; }

void specialDown(int k, int, int) {
	if (k == GLUT_KEY_UP)    specialKeys[0] = true;
	if (k == GLUT_KEY_DOWN)  specialKeys[1] = true;
	if (k == GLUT_KEY_LEFT)  specialKeys[2] = true;
	if (k == GLUT_KEY_RIGHT) specialKeys[3] = true;
}
void specialUp(int k, int, int) {
	if (k == GLUT_KEY_UP)    specialKeys[0] = false;
	if (k == GLUT_KEY_DOWN)  specialKeys[1] = false;
	if (k == GLUT_KEY_LEFT)  specialKeys[2] = false;
	if (k == GLUT_KEY_RIGHT) specialKeys[3] = false;
}

void updateMouse(int x, int y) {
	int w = glutGet(GLUT_WINDOW_WIDTH), h = glutGet(GLUT_WINDOW_HEIGHT);
	mouseX = (x / (float)w) * 2.0f - 1.0f;
	mouseY = 1.0f - (y / (float)h) * 2.0f;
}
void mouseMove(int x, int y) { updateMouse(x, y); }
void mouseClick(int button, int state, int x, int y) {
	updateMouse(x, y);
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) shoot();
}

// ----------------------------------------------------------------------------
//  Setup
// ----------------------------------------------------------------------------
void init() {
	glClearColor(0, 0, 0, 1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(-1, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);

	srand((unsigned)time(NULL));
	for (int i = 0; i < 60; i++) {
		starX[i] = rnd(-1, 1); starY[i] = rnd(-0.6f, 1); starP[i] = rnd(0, 6.28f);
	}
	resetGame();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); }

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(800, 800);
	glutInitWindowPosition(200, 50);
	glutCreateWindow("Spider Game - CT-384 CCA");

	init();
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyDown);
	glutKeyboardUpFunc(keyUp);
	glutSpecialFunc(specialDown);
	glutSpecialUpFunc(specialUp);
	glutPassiveMotionFunc(mouseMove);
	glutMotionFunc(mouseMove);
	glutMouseFunc(mouseClick);
	glutIgnoreKeyRepeat(1);
	glutTimerFunc(16, move, 0);        // start the animation loop

	glutMainLoop();
	return 0;
}