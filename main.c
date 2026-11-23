#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <math.h>

PSP_MODULE_INFO("BLOOD DEMON KNIGHT", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define LIST_SIZE (256 * 1024)

static unsigned int __attribute__((aligned(16))) list[LIST_SIZE];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

static Vertex cube[36];
static float px = 0.0f, pz = 0.0f;
static float camYaw = 0.0f;
static int attackFrames = 0;
static int demonFrames = 0;

static void makeCube(void) {
    const float p[8][3] = {
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},
        {0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},
        {0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}
    };
    const int idx[36] = {
        0,1,2, 0,2,3, 4,6,5, 4,7,6,
        0,4,5, 0,5,1, 3,2,6, 3,6,7,
        1,5,6, 1,6,2, 0,3,7, 0,7,4
    };
    int i;
    for (i=0; i<36; i++) {
        cube[i].color = 0xFF24242C;
        cube[i].x = p[idx[i]][0];
        cube[i].y = p[idx[i]][1];
        cube[i].z = p[idx[i]][2];
    }
}

static void drawCube(float x, float y, float z,
                     float sx, float sy, float sz, unsigned int color) {
    int i;
    for (i=0; i<36; i++) cube[i].color = color;
    sceGumPushMatrix();
    {
        ScePspFVector3 pos = {x,y,z};
        ScePspFVector3 scale = {sx,sy,sz};
        sceGumTranslate(&pos);
        sceGumScale(&scale);
        sceGumDrawArray(GU_TRIANGLES,
            GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            36, 0, cube);
    }
    sceGumPopMatrix();
}

static void drawTree(float x, float z) {
    drawCube(x, 0.85f, z, 0.22f, 1.7f, 0.22f, 0xFF493022);
    drawCube(x, 2.0f, z, 1.25f, 1.35f, 1.25f, 0xFF24452B);
    drawCube(x, 2.45f, z, 0.85f, 0.8f, 0.85f, 0xFF315A35);
}

static void drawNaito(void) {
    unsigned int armor = (demonFrames > 0) ? 0xFF351020 : 0xFF20202A;
    unsigned int cape  = (demonFrames > 0) ? 0xFFB51D32 : 0xFF711020;

    drawCube(0.0f, 0.95f, 0.0f, 0.52f, 0.72f, 0.30f, armor);
    drawCube(0.0f, 1.55f, 0.0f, 0.32f, 0.38f, 0.30f, 0xFFB98265);
    drawCube(0.0f, 1.76f, 0.02f, 0.38f, 0.16f, 0.34f, 0xFF151219);
    drawCube(-0.34f, 1.22f, 0.0f, 0.22f, 0.22f, 0.24f, 0xFF8D1826);
    drawCube( 0.34f, 1.22f, 0.0f, 0.22f, 0.22f, 0.24f, 0xFF8D1826);
    drawCube(-0.16f, 0.40f, 0.0f, 0.20f, 0.62f, 0.22f, armor);
    drawCube( 0.16f, 0.40f, 0.0f, 0.20f, 0.62f, 0.22f, armor);
    drawCube(-0.16f, 0.08f,-0.02f, 0.24f, 0.16f, 0.34f, 0xFF8D1826);
    drawCube( 0.16f, 0.08f,-0.02f, 0.24f, 0.16f, 0.34f, 0xFF8D1826);
    drawCube(0.0f, 0.80f, 0.25f, 0.60f, 0.82f, 0.08f, cape);
    drawCube(0.48f, 0.92f, 0.0f, 0.10f, 0.90f, 0.10f, 0xFFB8C0CC);
    drawCube(0.48f, 0.48f, 0.0f, 0.34f, 0.08f, 0.12f, 0xFFB58A35);
    drawCube(0.0f, 1.05f,-0.17f, 0.12f, 0.40f, 0.04f, 0xFFB51D32);

    if (attackFrames > 0) {
        drawCube(0.78f, 1.05f, -0.05f, 0.85f, 0.07f, 0.07f, 0xFFDFD9C8);
    }
}

static void setupGu(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH/2), 2048 - (SCR_HEIGHT/2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    SceCtrlData pad;
    int lastAttack = 0, lastDemon = 0;
    makeCube();
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    setupGu();

    while (1) {
        float dx = 0.0f, dz = 0.0f;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;

        /* Stick analogico y cruceta para desplazarse. */
        if (pad.Buttons & PSP_CTRL_LEFT)  dx -= 1.0f;
        if (pad.Buttons & PSP_CTRL_RIGHT) dx += 1.0f;
        if (pad.Buttons & PSP_CTRL_UP)    dz -= 1.0f;
        if (pad.Buttons & PSP_CTRL_DOWN)  dz += 1.0f;
        if (pad.Lx < 110) dx -= (110-pad.Lx)/110.0f;
        if (pad.Lx > 145) dx += (pad.Lx-145)/110.0f;
        if (pad.Ly < 110) dz -= (110-pad.Ly)/110.0f;
        if (pad.Ly > 145) dz += (pad.Ly-145)/110.0f;

        px += dx * 0.035f;
        pz += dz * 0.035f;

        /* Limites del pequeno claro de prueba. */
        if (px < -7.0f) px = -7.0f;
        if (px >  7.0f) px =  7.0f;
        if (pz < -7.0f) pz = -7.0f;
        if (pz >  7.0f) pz =  7.0f;

        if ((pad.Buttons & PSP_CTRL_CROSS) && !lastAttack) attackFrames = 12;
        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !lastDemon) demonFrames = (demonFrames > 0) ? 0 : 300;
        lastAttack = (pad.Buttons & PSP_CTRL_CROSS) != 0;
        lastDemon = (pad.Buttons & PSP_CTRL_CIRCLE) != 0;
        if (attackFrames > 0) attackFrames--;
        if (demonFrames > 0) demonFrames--;

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF30201C);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(55.0f, 16.0f/9.0f, 0.5f, 100.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        {
            ScePspFVector3 eye = {px, 3.0f, pz + 6.0f};
            ScePspFVector3 center = {px, 0.8f, pz};
            ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
            sceGumLookAt(&eye, &center, &up);
        }

        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();

        /* Suelo y arboles: escenario de bosque de prueba. */
        drawCube(0.0f, -0.18f, 0.0f, 18.0f, 0.18f, 18.0f, 0xFF3B3936);
        drawTree(-5.0f, -4.0f); drawTree(-3.5f, 3.0f);
        drawTree(4.8f, -3.8f);  drawTree(5.2f, 4.5f);
        drawTree(-6.0f, 5.0f);  drawTree(6.0f, -6.0f);

        sceGumPushMatrix();
        {
            ScePspFVector3 pos = {px, 0.0f, pz};
            sceGumTranslate(&pos);
            drawNaito();
        }
        sceGumPopMatrix();

        sceGuFinish();
        sceGuSync(0, 0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();

        pspDebugScreenSetXY(0,0);
        pspDebugScreenSetTextColor(0xFFFFFFFF);
        pspDebugScreenPrintf("BLOOD DEMON KNIGHT - PROTOTIPO 3D\n");
        pspDebugScreenPrintf("Stick/cruceta: mover | X: ataque | O: Runaibrut\n");
        pspDebugScreenPrintf("START: salir | Naito: modelo provisional\n");
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
