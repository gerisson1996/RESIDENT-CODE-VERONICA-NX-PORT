#include "../../../ps2/veronica/prog/binfunc.h"

// 100% matching!
int bhMlbBinRealize(void* bin_datP, ML_WORK* mlwP)
{
    unsigned int obj_num; 
    unsigned int tex_off; 
    unsigned int obj_off; 
    unsigned int status;  
    NJS_CNK_OBJECT* objP; 
    NJS_TEXNAME* namP;    
    unsigned int tex_num; 

    status = ((char*)bin_datP)[3];
    
    obj_num = ((unsigned short*)bin_datP)[3];
    
    tex_off = ((int*)bin_datP)[2];
    obj_off = ((int*)bin_datP)[3];
    
    mlwP->flg = 0;
    
    mlwP->obj_num = 0;
    
    mlwP->datP = S32(bin_datP);
    mlwP->objP = S32(NULL);
    mlwP->texP = S32(NULL);
    mlwP->owP = S32(NULL);
    
    bin_datP = (void*)((char*)bin_datP + ((unsigned short*)bin_datP)[2]);
    
    if (obj_off != -1) 
    {
        mlwP->flg = status;
        
        objP = (NJS_CNK_OBJECT*)PS2_HOST(((int)PS2_ADDR(bin_datP) + obj_off));
        
        mlwP->obj_num = obj_num;
        mlwP->objP = S32(objP);
        
        for ( ; obj_num != 0; obj_num--, objP++) 
        {
            if (P32(struct cnkobj *, objP->child) != (void*)-1) 
            {
                objP->child = S32((void*)PS2_HOST(((int)PS2_ADDR(P32(struct cnkobj *, objP->child)) + (int)PS2_ADDR(bin_datP))));
            } 
            else 
            {
                objP->child = S32(NULL);
            }
            
            if (P32(struct cnkobj *, objP->sibling) != (void*)-1) 
            {
                objP->sibling = S32((void*)PS2_HOST(((int)PS2_ADDR(P32(struct cnkobj *, objP->sibling)) + (int)PS2_ADDR(bin_datP))));
            }
            else 
            {
                objP->sibling = S32(NULL);
            }
            
            if (P32(NJS_CNK_MODEL *, objP->model) != (void*)-1) 
            {
                objP->model = S32((void*)PS2_HOST(((int)PS2_ADDR(P32(NJS_CNK_MODEL *, objP->model)) + (int)PS2_ADDR(bin_datP))));
                
                if (!(status & 0x80))
                {
                    bhBscBinRealize((NJS_MODEL*)P32(NJS_CNK_MODEL *, objP->model), (unsigned int)PS2_ADDR(bin_datP));
                } 
                else
                {
                    bhCnkBinRealize(P32(NJS_CNK_MODEL *, objP->model), (unsigned int)PS2_ADDR(bin_datP)); 
                }
            }
            else 
            {
                objP->model = S32(NULL);
            }
        }
    }
    
    if (tex_off != -1)
    {
        mlwP->texP = S32((void*)PS2_HOST(((int)PS2_ADDR(bin_datP) + tex_off)));
        
        P32(NJS_TEXLIST *, mlwP->texP)->textures = S32((NJS_TEXNAME*)((char*)P32(NJS_TEXNAME *, P32(NJS_TEXLIST *, mlwP->texP)->textures) + (int)PS2_ADDR(bin_datP)));
        
        namP = P32(NJS_TEXNAME *, P32(NJS_TEXLIST *, mlwP->texP)->textures);
        
        for (tex_num = P32(NJS_TEXLIST *, mlwP->texP)->nbTexture; tex_num != 0; tex_num--, namP++) 
        {
            namP->filename = S32((void*)((char*)P32(void *, namP->filename) + (int)PS2_ADDR(bin_datP)));
        }
    }
    
    return 1;
}

// 100% matching!
int bhBscBinRealize(NJS_MODEL* mdlP, unsigned int dat_off)
{
    NJS_MESHSET* mshP;
    unsigned int i;
    
    if ((mdlP->nbPoint & 0x80000000))
    {
        mdlP->nbPoint &= ~0x80000000;
        
        if ((int)PS2_ADDR(P32(NJS_POINT3 *, mdlP->points)) != -1) 
        {
            mdlP->points = S32((NJS_POINT3*)((char*)P32(NJS_POINT3 *, mdlP->points) + dat_off));
        }
        
        if ((int)PS2_ADDR(P32(NJS_POINT3 *, mdlP->normals)) != -1) 
        {
            mdlP->normals = S32((NJS_VECTOR*)((char*)P32(NJS_POINT3 *, mdlP->normals) + dat_off));
        }
        
        if ((int)PS2_ADDR(P32(NJS_MESHSET *, mdlP->meshsets)) != -1) 
        {
            mshP = P32(NJS_MESHSET *, mdlP->meshsets = S32((NJS_MESHSET*)((char*)P32(NJS_MESHSET *, mdlP->meshsets) + dat_off)));
        }
        
        if ((int)PS2_ADDR(P32(NJS_MATERIAL *, mdlP->mats)) != -1)
        {
            mdlP->mats = S32((NJS_MATERIAL*)((char*)P32(NJS_MATERIAL *, mdlP->mats) + dat_off));
        }
        
        for (i = 0; i < mdlP->nbMeshset; i++, mshP++) 
        {
            if ((int)PS2_ADDR(P32(short *, mshP->meshes)) != -1) 
            {
                mshP->meshes = S32((short*)((char*)P32(short *, mshP->meshes) + dat_off));
            }
            
            if ((int)PS2_ADDR(P32(unsigned int *, mshP->attrs)) != -1) 
            {
                mshP->attrs = S32((unsigned int*)((char*)P32(unsigned int *, mshP->attrs) + dat_off));
            }
            else 
            {
                mshP->attrs = S32(NULL);
            }
            
            if ((int)PS2_ADDR(P32(NJS_POINT3 *, mshP->normals)) != -1)
            {
                mshP->normals = S32((NJS_VECTOR*)((char*)P32(NJS_POINT3 *, mshP->normals) + dat_off));
            } 
            else
            {
                mshP->normals = S32(NULL);
            }
            
            if ((int)PS2_ADDR(P32(NJS_COLOR *, mshP->vertcolor)) != -1) 
            {
                mshP->vertcolor = S32((NJS_COLOR*)((char*)P32(NJS_COLOR *, mshP->vertcolor) + dat_off));
            }
            else 
            {
                mshP->vertcolor = S32(NULL);
            }
            
            if ((int)PS2_ADDR(P32(NJS_COLOR *, mshP->vertuv)) != -1) 
            {
                mshP->vertuv = S32((NJS_COLOR*)((char*)P32(NJS_COLOR *, mshP->vertuv) + dat_off));
            } 
            else
            {
                mshP->vertuv = S32(NULL);
            }
        }
    }
    
    return 1;
}

// 100% matching!
int bhCnkBinRealize(NJS_CNK_MODEL* mdlP, unsigned int dat_off)
{
    if (mdlP->r < 0) 
    {
        mdlP->r = -mdlP->r; 
        
        if ((int)PS2_ADDR(P32(int *, mdlP->vlist)) != -1)
        { 
            mdlP->vlist = S32((int*)((char*)P32(int *, mdlP->vlist) + dat_off)); 
        }
        
        if ((int)PS2_ADDR(P32(short *, mdlP->plist)) != -1)
        {
            mdlP->plist = S32((short*)((char*)P32(short *, mdlP->plist) + dat_off)); 
        }
    }
    
    return 1; 
}

// 100% matching!
int bhMnbBinRealize(void* bin_datP, MN_WORK* mnwP)
{
    NJS_MOTION* mtnP;     
    NJS_MDATA2_MOD* md2P;
    void* dat_topP;     
    int i;               
    
    dat_topP = (void*)PS2_HOST(((int)PS2_ADDR(bin_datP) + ((unsigned short*)bin_datP)[2]));
    mtnP = (NJS_MOTION*)PS2_HOST(((int)PS2_ADDR(dat_topP) + ((int*)bin_datP)[2]));
    md2P = (NJS_MDATA2_MOD*)PS2_HOST(((int)PS2_ADDR(P32(void *, mtnP->mdata)) + (int)PS2_ADDR(dat_topP)));
    
    mtnP->mdata = S32(md2P);
    
    mnwP->flg = ((char*)bin_datP)[3];
    
    mnwP->obj_num = ((unsigned short*)bin_datP)[3];
    mnwP->frm_num = mtnP->nbFrame;
    
    mnwP->datP = S32(bin_datP);
    mnwP->md2P = S32((NJS_MDATA2_MOD*)md2P);
    mnwP->atrP = S32(NULL);
    
    if (((int*)bin_datP)[3] != -1) 
    {
        mnwP->atrP = S32((unsigned short*)PS2_HOST(((int)PS2_ADDR(dat_topP) + ((int*)bin_datP)[3])));
    }
    
    for (i = 0; i < ((unsigned short*)bin_datP)[3]; i++, md2P++)
    {
        if (P32(void *, md2P->p[0]) == (void*)-1) 
        {
            md2P->p[0] = S32(NULL);
        } 
        else 
        {
            md2P->p[0] = S32((void*)((char*)P32(void *, md2P->p[0]) + (int)PS2_ADDR(dat_topP)));
        }
        
        if (P32(void *, md2P->p[1]) == (void*)-1) 
        {
            md2P->p[1] = S32(NULL);
        } 
        else 
        {
            md2P->p[1] = S32((void*)((char*)P32(void *, md2P->p[1]) + (int)PS2_ADDR(dat_topP)));
        }
    }
    
    return 1;
}
