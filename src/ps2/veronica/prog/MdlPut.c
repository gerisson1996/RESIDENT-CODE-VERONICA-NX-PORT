#include "../../../ps2/veronica/prog/MdlPut.h"
#include "../../../ps2/veronica/prog/njplus.h"
#include "../../../ps2/veronica/prog/ps2_NaMatrix.h"
#include "../../../ps2/veronica/prog/ps2_NaTextureFunction.h"
#include "../../../ps2/veronica/prog/ps2_NinjaCnk.h"
#include "../../../ps2/veronica/prog/main.h"

void (*bhJumpCnkDraw[4])(BH_PWORK* ewP, NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num) = 
{ 
	EasyMultiDrawTreeCnk, 
	SimpleMultiDrawTreeCnk, 
	EasyDrawTreeCnk, 
	SimpleDrawTreeCnk 
};

// 100% matching! 
#ifdef TARGET_PC
extern int pose_trace_mark;
void pose_trace(BH_PWORK* p);
int pc_zombie_head(BH_PWORK* ep, int hide);   /* pc/glue/variant_glue.c */
#endif
void bhPutModel(BH_PWORK* ewP) 
{
    // these variables are arranged in an order different from the debugging symbols
    O_WORK* owP;       
    int draw_tp;       
    ML_WORK* mlwP;     
    unsigned int c3df; 
    
#ifdef TARGET_PC
    static int pc_headless;
    if (!pc_headless && pc_zombie_head(ewP, 1))   /* a zombie whose head was blown off: drawn without it */
    {
        pc_headless = 1;
        bhPutModel(ewP);
        pc_headless = 0;
        pc_zombie_head(ewP, 0);
        return;
    }
    if (pose_trace_mark)
    {
        pose_trace(ewP);   /* test aid RECVX_POSE_TRACE (pc/glue/pose_trace.c) */
    }
#endif
    mlwP = P32(struct ML_WORK *, ewP->mlwP);
    owP = P32(struct O_WORK *, mlwP->owP);
    
    if (P32(NJS_TEXLIST *, mlwP->texP) != NULL) 
    {
        njSetTexture(P32(NJS_TEXLIST *, mlwP->texP));
    }
    
    draw_tp = ewP->draw_tp;
    
    if (((sys->gm_flg & 0x80)) && (draw_tp < 2)) 
    {
        draw_tp += 2;
    }
    
    if ((mlwP->flg & 0x80))
    {
        if (P32(int *, ewP->skp[ewP->mdl_no]) != NULL) 
        {
            npPushMdlstr(P32(struct cnkobj *, mlwP->objP), mlwP->obj_num);
            
            if ((ewP->mdflg & 0x100))
            {
                npCalcSkinFM(ewP, mlwP->obj_num, P32(int *, ewP->skp[ewP->mdl_no]));
            } 
            else 
            {
                npCalcSkin(ewP, mlwP->obj_num, P32(int *, ewP->skp[ewP->mdl_no]));
            }
            
            bhJumpCnkDraw[draw_tp](ewP, P32(struct cnkobj *, mlwP->objP), owP, mlwP->obj_num);
            
            if (((sys->st_flg & 0x100)) && (!(ewP->mdflg & 0x40))) 
            {
                njSetMatrix(P32(float (*)[16], cam.mtxb), P32(float (*)[16], cam.mtx));
                
                njMirror(P32(float (*)[16], cam.mtx), &sys->mr_pl);
                
                njSetMatrix(NULL, P32(float (*)[16], cam.mtx));
                
                njCnkSetEasyMultiLightMatrices();
                njCnkSetSimpleMultiLightMatrices();
                
                c3df = _nj_control_3d_flag_;
                
                njControl3D(c3df | 0x10000);
                
                bhJumpCnkDraw[draw_tp](ewP, P32(struct cnkobj *, mlwP->objP), owP, mlwP->obj_num);
                
                njControl3D(c3df);
                
                njSetMatrix(P32(float (*)[16], cam.mtx), P32(float (*)[16], cam.mtxb));
                njSetMatrix(NULL, P32(float (*)[16], cam.mtx));
                
                njCnkSetEasyMultiLightMatrices();
                njCnkSetSimpleMultiLightMatrices();
            }
            
            npPopMdlstr(P32(struct cnkobj *, mlwP->objP), mlwP->obj_num);
        }
        else 
        {
            bhJumpCnkDraw[draw_tp](ewP, P32(struct cnkobj *, mlwP->objP), owP, mlwP->obj_num);
            
            if (((sys->st_flg & 0x100)) && (!(ewP->mdflg & 0x40))) 
            {
                njSetMatrix(P32(float (*)[16], cam.mtxb), P32(float (*)[16], cam.mtx));
                
                njMirror(P32(float (*)[16], cam.mtx), &sys->mr_pl);
                
                njSetMatrix(NULL, P32(float (*)[16], cam.mtx));
                
                njCnkSetEasyMultiLightMatrices();
                njCnkSetSimpleMultiLightMatrices();

                c3df = _nj_control_3d_flag_;
                
                njControl3D(c3df | 0x10000);
                
                bhJumpCnkDraw[draw_tp](ewP, P32(struct cnkobj *, mlwP->objP), owP, mlwP->obj_num);
                
                njControl3D(c3df);
                
                njSetMatrix(P32(float (*)[16], cam.mtx), P32(float (*)[16], cam.mtxb));
                njSetMatrix(NULL, P32(float (*)[16], cam.mtx));
                
                njCnkSetEasyMultiLightMatrices();
                njCnkSetSimpleMultiLightMatrices();
            }
        }
    }
    else 
    {
        DrawTreeBsc(P32(struct cnkobj *, mlwP->objP), owP, mlwP->obj_num);
    }
}

// 100% matching!
static void DrawTreeBsc(NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num)
{
    for ( ; obj_num > 0; obj_num--, objP++, owP++) 
    {
        njPushMatrix(P32(float (*)[16], cam.mtx));
        njMultiMatrix(NULL, &owP->mtx);
        
        if ((P32(NJS_CNK_MODEL *, objP->model) != NULL) && (!(objP->evalflags & 0x8))) 
        {
            njDrawModel((NJS_MODEL*)P32(NJS_CNK_MODEL *, objP->model));
        }
        
        njPopMatrixEx();
    } 
}

// 100% matching! 
static void EasyMultiDrawTreeCnk(BH_PWORK* ewP, NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num)
{
    for ( ; obj_num > 0; obj_num--, objP++, owP++) 
    {
        njPushMatrix(P32(float (*)[16], cam.mtx));
        
        if (!(objP->evalflags & 0x80000000))
        {
            njMultiMatrix(NULL, &owP->mtx);
        } 
        else 
        {
            njTranslate(NULL, ewP->px, ewP->py, ewP->pz);
        }
        
        if ((P32(NJS_CNK_MODEL *, objP->model) != NULL) && (!(objP->evalflags & 0x8)))
        {
            njCnkEasyMultiDrawModel(P32(NJS_CNK_MODEL *, objP->model));
        }
        
        njPopMatrixEx();
    } 
}

// 100% matching! 
static void SimpleMultiDrawTreeCnk(BH_PWORK* ewP, NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num)
{
    for ( ; obj_num > 0; obj_num--, objP++, owP++) 
    {
        njPushMatrix(P32(float (*)[16], cam.mtx));
        
        if (!(objP->evalflags & 0x80000000))
        {
            njMultiMatrix(NULL, &owP->mtx);
        } 
        else 
        {
            njTranslate(NULL, ewP->px, ewP->py, ewP->pz);
        }
        
        if ((P32(NJS_CNK_MODEL *, objP->model) != NULL) && (!(objP->evalflags & 0x8)))
        {
            njCnkSimpleMultiDrawModel(P32(NJS_CNK_MODEL *, objP->model));
        }
        
        njPopMatrixEx();
    } 
}

// 100% matching! 
static void EasyDrawTreeCnk(BH_PWORK* ewP, NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num)
{
    for ( ; obj_num > 0; obj_num--, objP++, owP++) 
    {
        njPushMatrix(P32(float (*)[16], cam.mtx));
        
        if (!(objP->evalflags & 0x80000000))
        {
            njMultiMatrix(NULL, &owP->mtx);
        } 
        else 
        {
            njTranslate(NULL, ewP->px, ewP->py, ewP->pz);
        }
        
        if ((P32(NJS_CNK_MODEL *, objP->model) != NULL) && (!(objP->evalflags & 0x8)))
        {
            njCnkEasyDrawModel(P32(NJS_CNK_MODEL *, objP->model));
        }
        
        njPopMatrixEx();
    } 
}

// 100% matching! 
static void SimpleDrawTreeCnk(BH_PWORK* ewP, NJS_CNK_OBJECT* objP, O_WORK* owP, int obj_num)
{
    for ( ; obj_num > 0; obj_num--, objP++, owP++) 
    {
        njPushMatrix(P32(float (*)[16], cam.mtx));
        
        if (!(objP->evalflags & 0x80000000))
        {
            njMultiMatrix(NULL, &owP->mtx);
        } 
        else 
        {
            njTranslate(NULL, ewP->px, ewP->py, ewP->pz);
        }
        
        if ((P32(NJS_CNK_MODEL *, objP->model) != NULL) && (!(objP->evalflags & 0x8)))
        {
            njCnkSimpleDrawModel(P32(NJS_CNK_MODEL *, objP->model));
        }
        
        njPopMatrixEx();
    } 
}

// 100% matching! 
void bhCalcModel(BH_PWORK* ewP) 
{
    if ((ewP->flg & 0x80))
    {
        njSetMatrix(P32(float (*)[16], ewP->mtx), &P32(struct O_WORK *, P32(struct ML_WORK *, ((BH_PWORK*)P32(unsigned char *, ewP->lkwkp))->mlwP)->owP)[ewP->lkono].mtx);
        
        njTranslate(P32(float (*)[16], ewP->mtx), ewP->lox, ewP->loy, ewP->loz);
        
        if ((ewP->flg & 0x1000)) 
        {
            njUnitRotPortion(P32(float (*)[16], ewP->mtx));
        }
        
        njRotateXYZ(P32(float (*)[16], ewP->mtx), ewP->ax, ewP->ay, ewP->az);
    }
    else if (!(ewP->flg & 0x1000))
    {
        njUnitMatrix(P32(float (*)[16], ewP->mtx));
        
        njTranslate(P32(float (*)[16], ewP->mtx), ewP->px, ewP->py, ewP->pz);
        
        njRotateXYZ(P32(float (*)[16], ewP->mtx), ewP->ax, ewP->ay, ewP->az);
    }
    
    bhCalcTree(P32(float (*)[16], ewP->mtx), P32(struct ML_WORK *, ewP->mlwP));
}

// 100% matching!
void bhCalcTree(NJS_MATRIX* basP, ML_WORK* mlwP) 
{
    O_WORK* owP;
    NJS_CNK_OBJECT* objP;
    int obj_num;

    owP = P32(struct O_WORK *, mlwP->owP);
    objP = P32(struct cnkobj *, mlwP->objP);

    njPushMatrix(basP);

    if (basP == NULL)
    {
        njUnitMatrix(NULL);
    }

    obj_num = mlwP->obj_num;

    for ( ; obj_num > 0; obj_num--, objP++, owP++)
    {
        if (P32(struct cnkobj *, objP->sibling) != NULL)
        {
            njPushMatrixEx();
        }

        njTranslate(NULL, objP->pos[0], objP->pos[1], objP->pos[2]);
        njRotateXYZ(NULL, objP->ang[0], objP->ang[1], objP->ang[2]);

        if (!(owP->flg & 0x1)) 
        {
            njGetMatrix(&owP->mtx);
        }
        else 
        {
            njSetMatrix(NULL, &owP->mtx);
        }

        if (P32(struct cnkobj *, objP->child) == NULL)
        {
            njPopMatrixEx();
        }
    }

    njPopMatrixEx();
}
