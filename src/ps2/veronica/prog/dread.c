#include "../../../ps2/veronica/prog/dread.h"
#include "../../../ps2/veronica/prog/binfunc.h"
#include "../../../ps2/veronica/prog/njplus.h"
#include "../../../ps2/veronica/prog/ps2_NaTextureFunction.h"
#include "../../../ps2/veronica/prog/ps2_texture.h"
#include "../../../ps2/veronica/prog/pwksub.h"
#include "../../../ps2/veronica/prog/weapon.h"
#include "../../../ps2/veronica/prog/main.h"

/*char* mshp_tab[16]; - unused*/

// 100% matching!
void bhReadPlayerData()
{
    ML_WORK* mdlp;       
    MN_WORK* mtnp;       
    int i;                
    int plmdlsz;          
    unsigned int dt0;     
    unsigned int dt1;      
    unsigned char* datp;   
    unsigned char* plmdlp; 
    unsigned char* mempb;  
    unsigned char* plmtnp; 
    int temp; // not from the debugging symbols

    mempb = P32(unsigned char *, sys->memp);
    datp = P32(unsigned char *, sys->memp);
    
    dt0 = *(unsigned int*)datp;
    
    datp += 4;
    
    njMemCopy(P32(unsigned char *, sys->plmdlp), datp, dt0);
    
    plmdlp = P32(unsigned char *, sys->plmdlp);
    
    plp->mlwP = S32(plp->mdl);
    
    mdlp = plp->mdl;
    
    plp->mdl_n = 0;
    
    while ((dt1 = *(unsigned int*)plmdlp) != -1)
    {
        if (dt1 != 0)
        {
            plmdlp += 4;
            
            if (*(unsigned int*)plmdlp == SKIN_MAGIC) 
            {
                plp->skp[plp->mdl_n] = S32((int*)&plmdlp[4]);
            }
            else
            {
                bhMlbBinRealize(plmdlp, mdlp);
                
                if (P32(int *, plp->skp[plp->mdl_n]) != NULL) 
                {
                    npSkinConvert(P32(struct cnkobj *, mdlp->objP), P32(int *, plp->skp[plp->mdl_n]));
                }
                
                plp->mbp[plp->mdl_n] = S32(P32(struct cnkobj *, mdlp->objP));
                plp->txp[plp->mdl_n] = S32(P32(NJS_TEXLIST *, mdlp->texP));
                
                mdlp++;
                
                plp->mdl_n++;
            }
            
            plmdlp = &plmdlp[dt1];
        } 
        else 
        {
            plmdlp += 4;
        }
    }
    
    datp = &datp[dt0];  
    
    dt1 = *(unsigned int*)datp;
    
    datp += 4;
    
    njMemCopy(P32(unsigned char *, sys->plbmtp), datp, dt1);
    
    sys->bmt_size = dt1;
    
    plmtnp = P32(unsigned char *, sys->plbmtp);
    
    plp->mnwP = S32((MN_WORK*)P32(unsigned char *, sys->plmthp));
    plp->mnwPb = S32(P32(struct MN_WORK *, plp->mnwP));
    
    mtnp = P32(struct MN_WORK *, plp->mnwP);
    
    while ((temp = *(unsigned int*)plmtnp) != -1) 
    {
        if (temp != 0) 
        {
            plmtnp += 4;
            
            bhMnbBinRealize(plmtnp, mtnp);
            
            plmtnp = &plmtnp[temp]; 
        } 
        else
        {
            mtnp->md2P = S32(NULL);
            
            plmtnp += 4;
        }
        
        mtnp++;
    }

    datp = &datp[dt1];

    plmdlsz = *(unsigned int*)datp;
    
    datp += 4;
    
    njMemCopy(P32(unsigned char *, sys->plzmtp), datp, plmdlsz);
    
    datp = &datp[plmdlsz];
    
    for (i = 0; i < plp->mdl_n; i++) 
    {
        if (P32(NJS_TEXLIST *, plp->mdl[i].texP) != NULL) 
        {
            temp = *(unsigned int*)datp;
            
            if ((temp & 0x80000000)) 
            {
                datp += 4;
                
                datp = (unsigned char*)PS2_HOST((((int)PS2_ADDR(datp) + 31) & ~0x1F));
                
                temp &= ~0x80000000;
            } 
            else 
            {
                datp += 4;
            }
            
            plp->mdl[i].flg |= 0x200;
            
            bhSetMemPvpTexture(P32(NJS_TEXLIST *, plp->mdl[i].texP), datp, 0);
            
            datp = &datp[temp];
        } 
        else 
        {
            plp->mdl[i].flg &= ~0x200;
        }
    }
    
    sys->memp = S32((unsigned char*)PS2_HOST((((int)PS2_ADDR(&P32(unsigned char *, sys->plmdlp)[dt0]) + 256) & ~0xFF)));
    
    for (i = 0; i < plp->mdl_n; i++) 
    {
        plp->mdl[i].owP = S32((O_WORK*)bhGetFreeMemory(plp->mdl[i].obj_num * sizeof(O_WORK), 32));
    }
    
    sys->hd_pos = *(NJS_POINT3*)&P32(struct cnkobj *, P32(struct ML_WORK *, plp->mlwP)->objP)[5].pos;
    
    sys->memp = S32(mempb);
}

// 100% matching! 
void bhReadWeaponData()
{
    O_WRK* op;             
    int sz;              
    int msz;               
    int wpn_no;            
    unsigned char* dp;   
    unsigned char* mp;   
    MN_WORK* mtnp;         
    int mtsz;              
    unsigned char* plmtnp; 
    int temp; // not from the debugging symbols

    wpn_no = plp->wpnr_no;
    
    dp = P32(unsigned char *, sys->memp);
    
    msz = *(unsigned int*)dp;
    
    dp += 4;
    
    op = P32(struct O_WRK *, sys->obwp);
    
    if ((!(sys->ss_flg & 0x100)) && (P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP) != NULL)) 
    {
        njReleaseTexture(P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP));
        
        bhGarbageTexture(NULL, 0);
    }
    
    if (msz != 0) 
    {
        mp = P32(unsigned char *, sys->wrmdlp);
        
        npCopyMemory(mp, dp, msz);
        
        bhSetWeapon(op, wpn_no, 0);
        
        op->mlwP = S32(op->mdl);
        
        bhMlbBinRealize(mp, P32(struct ML_WORK *, op->mlwP));
    }
    
    dp = &dp[msz];
    
    temp = *(unsigned int*)dp;
    
    if ((temp & 0x80000000))
    {
        dp += 4;
        
        dp = (unsigned char*)PS2_HOST((((int)PS2_ADDR(dp) + 63) & ~0x3F));
        
        temp &= ~0x80000000;
    }
    else 
    {
        dp += 4;
    }
    
    if (temp != 0) 
    {
        if (P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP) != NULL)
        {
            P32(struct ML_WORK *, op->mlwP)->flg |= 0x200;
            
            bhSetMemPvpTexture(P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP), dp, 0);
        }
        
        P32(struct ML_WORK *, op->mlwP)->owP = S32((O_WORK*)PS2_HOST((((int)PS2_ADDR(&mp[msz]) + 255) & ~0xFF)));
        
        memset(P32(struct O_WORK *, P32(struct ML_WORK *, op->mlwP)->owP), 0, P32(struct ML_WORK *, op->mlwP)->obj_num * 80);
        
        op->mdl_no = 0;
    }
    
    dp = &dp[temp];
    
    msz = *(unsigned int*)dp;
    
    dp += 4;
    
    op = &P32(struct O_WRK *, sys->obwp)[1]; 
    
    if ((!(sys->ss_flg & 0x100)) && (P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP) != NULL))
    {
        njReleaseTexture(P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP));
        
        bhGarbageTexture(NULL, 0);
    }
    
    if (msz != 0) 
    {
        mp = P32(unsigned char *, sys->wlmdlp);
        
        npCopyMemory(mp, dp, msz);
        
        bhSetWeapon(op, wpn_no, 1);
        
        op->mlwP = S32(op->mdl);
        
        bhMlbBinRealize(mp, P32(struct ML_WORK *, op->mlwP));
    }
    
    dp = &dp[msz];
    
    sz = *(unsigned int*)dp;
    
    if ((sz & 0x80000000))
    {
        dp += 4;
        
        dp = (unsigned char*)PS2_HOST((((int)PS2_ADDR(dp) + 63) & ~0x3F));
        
        sz &= ~0x80000000;
    }
    else  
    {
        dp += 4;
    }
    
    if (sz != 0)
    {
        if (P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP) != NULL)
        {
            P32(struct ML_WORK *, op->mlwP)->flg |= 0x200;
            
            bhSetMemPvpTexture(P32(NJS_TEXLIST *, P32(struct ML_WORK *, op->mlwP)->texP), dp, 0);
        }
        
        P32(struct ML_WORK *, op->mlwP)->owP = S32((O_WORK*)PS2_HOST((((int)PS2_ADDR(&mp[msz]) + 255) & ~0xFF)));
        
        memset(P32(struct O_WORK *, P32(struct ML_WORK *, op->mlwP)->owP), 0, P32(struct ML_WORK *, op->mlwP)->obj_num * 80);
        
        op->mdl_no = 0;
    }
    
    dp = &dp[sz];
    
    sz = *(unsigned int*)dp;
    
#ifdef TARGET_PC   /* a file without the reload motion (116: knife, Gold Lugers, SMG, M-100P, linear and rocket launcher) skips
                      its entry, which kept the previous weapon's, pointing into this buffer now overwritten (or NULL):
                      empty, bhCPM2_act_rld reloads without a motion */
    if (P32(struct MN_WORK *, plp->mnwP) != NULL) memset(&P32(struct MN_WORK *, plp->mnwP)[116], 0, sizeof(P32(struct MN_WORK *, plp->mnwP)[116]));
#endif
    if (sz != 0) 
    {
        plmtnp = P32(unsigned char *, sys->plwmtp);

        dp += 4;
        
        npCopyMemory(plmtnp, dp, sz);
        
        sys->wmt_size = sz;
        
        for (mtnp = &P32(struct MN_WORK *, plp->mnwP)[100]; (mtsz = *(unsigned int*)plmtnp) != -1; mtnp++) 
        {
            if (mtsz != 0) 
            {
                dp = plmtnp + 4;
                
                bhMnbBinRealize(dp, mtnp);
                
                plmtnp = &dp[mtsz];
            } 
            else 
            {
                plmtnp += 4;
            }
        }
    }
}

// 100% matching!
unsigned char* bhKeepObjWork(ML_WORK* mp, unsigned char* sp)
{
    sp = (unsigned char*)PS2_HOST((((int)PS2_ADDR(sp) + 15) & ~0xF));
    
    mp->owP = S32((O_WORK*)sp);
    
    memset(sp, 0, mp->obj_num * 80);
    
    sp += mp->obj_num * 80;
    
    return sp;
}
