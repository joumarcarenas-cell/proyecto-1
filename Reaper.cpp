#include "include/Reaper.h"
#include "include/Boss.h"
#include "include/graphics/VFXSystem.h"
#include "include/graphics/AnimeVFX.h"
#include "include/CombatUtils.h"
#include <cmath>
#include <algorithm>
extern float screenShake;
extern float hitstopTimer;

// Draw isometric arc sector
static void DrawIsoSector(Vector2 c,float r,float a1,float a2,Color col){
    int N=24; float step=(a2-a1)/N;
    for(int i=0;i<N;i++){
        float f1=(a1+i*step)*DEG2RAD, f2=(a1+(i+1)*step)*DEG2RAD;
        Vector2 v1={c.x+cosf(f1)*r, c.y+sinf(f1)*r*0.5f};
        Vector2 v2={c.x+cosf(f2)*r, c.y+sinf(f2)*r*0.5f};
        DrawTriangle(c,v1,v2,col); DrawTriangle(c,v2,v1,col);
        DrawLineEx(v1,v2,2.f,Fade(WHITE,col.a/255.f));
    }
}

// Scythe shape
static void DrawScythe(Vector2 p,float a,float sz,Color c){
    Vector2 tip={p.x+cosf(a)*sz, p.y+sinf(a)*sz};
    Vector2 l={p.x+cosf(a+2.4f)*sz*.7f, p.y+sinf(a+2.4f)*sz*.7f};
    Vector2 r2={p.x+cosf(a-2.4f)*sz*.7f, p.y+sinf(a-2.4f)*sz*.7f};
    DrawTriangle(p,l,tip,c); DrawTriangle(p,tip,r2,c);
    DrawTriangle(p,tip,l,c); DrawTriangle(p,r2,tip,c);
    DrawLineEx(tip,l,2.5f,WHITE); DrawLineEx(tip,r2,2.5f,WHITE);
}

// VFX: slash burst
static void VfxSlash(Vector2 pos,Vector2 dir,Color col){
    auto& V=Graphics::VFXSystem::GetInstance();
    float base=atan2f(dir.y,dir.x);
    for(int i=0;i<12;i++){
        float a=base+GetRandomValue(-60,60)*DEG2RAD, s=(float)GetRandomValue(350,800);
        V.SpawnPremium(pos,{cosf(a)*s,sinf(a)*s},{0,0},0.25f,col,Fade(WHITE,0),(float)GetRandomValue(4,10),0,Graphics::RenderType::RHOMB,BLEND_ADDITIVE,Graphics::EasingType::EASE_OUT_EXPO);
    }
    AnimeVFX::AnimeEmitter::SpawnAnimeImpact(Vector2Add(pos,Vector2Scale(dir,35.f)),{100,0,150,255});
}

// VFX: heavy slash
static void VfxHeavy(Vector2 pos,Vector2 dir){
    auto& V=Graphics::VFXSystem::GetInstance();
    for(int i=0;i<20;i++){
        float a=atan2f(dir.y,dir.x)+GetRandomValue(-45,45)*DEG2RAD, s=(float)GetRandomValue(400,1100);
        V.SpawnPremium(pos,{cosf(a)*s,sinf(a)*s-600},{0,900},0.45f,{180,0,255,255},Fade(WHITE,0),(float)GetRandomValue(7,16),0,Graphics::RenderType::SDF_CIRCLE,BLEND_ADDITIVE,Graphics::EasingType::EASE_OUT_EXPO);
    }
    AnimeVFX::AnimeEmitter::SpawnAnimeImpact(pos,{80,0,120,255});
}

// VFX: blood thorn spikes ascending (E explosion)
static void VfxBloodThorns(Vector2 pos){
    auto& V=Graphics::VFXSystem::GetInstance();
    // Ring of thorns shooting upward
    for(int i=0;i<16;i++){
        float a=(float)i/16.f*PI*2.f;
        float r=(float)GetRandomValue(30,180);
        Vector2 sp={pos.x+cosf(a)*r, pos.y+sinf(a)*r*0.5f};
        // Main spike shooting up
        V.SpawnPremium(sp,{cosf(a)*80.f,-600.f+sinf(a)*50.f},{0,1200},0.55f,{220,0,40,255},{60,0,10,0},
            (float)GetRandomValue(10,22),(float)GetRandomValue(2,6),
            Graphics::RenderType::RHOMB,BLEND_ADDITIVE,Graphics::EasingType::EASE_OUT_EXPO,
            0.9f,0.f,(float)GetRandomValue(-30,30));
        // Blood drops
        V.SpawnPremium(sp,{cosf(a)*200.f+(float)GetRandomValue(-80,80),(float)GetRandomValue(-400,-100)},{0,600},0.4f,
            {180,0,20,200},{40,0,5,0},(float)GetRandomValue(4,9),0,
            Graphics::RenderType::SDF_CIRCLE,BLEND_ALPHA,Graphics::EasingType::EASE_OUT_QUAD);
    }
    // No SonicBoom - just the blood impact burst
    Graphics::SpawnImpactBurst(pos,{0,1},{200,0,50,255},{60,0,10,255},20,10);
    screenShake=1.2f;
}

// VFX: blood pool on floor
static void DrawBloodPool(Vector2 pos,float alpha){
    DrawEllipse((int)pos.x,(int)pos.y,32,16,Fade({130,0,20,200},alpha));
    DrawEllipseLines((int)pos.x,(int)pos.y,32,16,Fade({200,0,40,255},alpha));
}

void Reaper::ApplyBleed(Boss& boss) const {
    if(boss.desperationResists) return;
    boss.isBleeding=true; boss.bleedTimer=10.f;
    boss.bleedTickTimer=0.5f; boss.bleedTotalDamage=8.f+rpg.statFuerza*1.5f;
}

void Reaper::StartDashAttack(Vector2 targetPos,bool isUlt){
    dashAtk.active=true; dashAtk.isUltimate=isUlt;
    dashAtk.start=position; dashAtk.end=targetPos;
    dashAtk.duration=isUlt?0.28f:0.32f;
    dashAtk.timer=0.f; dashAtk.slashTimer=0.f; dashAtk.hitEnemy=false;
    // Boom on cast
    Graphics::SpawnSonicBoom(position,isUlt?160.f:100.f);
    if(isUlt) screenShake=0.4f;
}

void Reaper::Update(){
    float dt=GetFrameTime()*g_timeScale;
    if(isAdminMode){qCooldown=eCooldown=ultCooldown=0;energy=maxEnergy;hp=maxHp;dashCharges=maxDashCharges;}
    else{if(qCooldown>0)qCooldown-=dt;if(eCooldown>0)eCooldown-=dt;if(ultCooldown>0)ultCooldown-=dt;}
    if(hitCooldownTimer>0)hitCooldownTimer-=dt;
    if(isStaggered){staggerTimer-=dt;if(staggerTimer<=0)isStaggered=false;return;}

    // E stun
    if(eStunTimer>0){
        eStunTimer-=dt;
        // VFX: thorns growing while charging (spines appear during stun)
        if(GetRandomValue(0,100)<50){
            float a=(float)GetRandomValue(0,360)*DEG2RAD;
            float r=(float)GetRandomValue(20,160);
            Vector2 sp={position.x+cosf(a)*r, position.y+sinf(a)*r*0.5f};
            Graphics::VFXSystem::GetInstance().SpawnPremium(sp,{cosf(a)*30.f,-400.f},{0,800},0.4f,
                {200,0,30,255},{50,0,5,0},GetRandomValue(6,14),GetRandomValue(1,4),
                Graphics::RenderType::RHOMB,BLEND_ADDITIVE,Graphics::EasingType::EASE_OUT_EXPO);
        }
        if(eStunTimer<=0){state=ReaperState::NORMAL;eExplosionReady=true;VfxBloodThorns(position);}
        return;
    }

    // DashAttack (Q/R)
    if(dashAtk.active){
        dashAtk.timer+=dt;
        float p=dashAtk.timer/dashAtk.duration;
        if(p>=1.f){p=1.f;dashAtk.active=false;}
        position=Arena::GetClampedPos(Vector2Lerp(dashAtk.start,dashAtk.end,p),radius);
        facing=Vector2Normalize(Vector2Subtract(dashAtk.end,dashAtk.start));

        // Slash echo every 0.05s
        dashAtk.slashTimer+=dt;
        if(dashAtk.slashTimer>=0.05f){
            dashAtk.slashTimer=0.f;
            Color ec=dashAtk.isUltimate?Color{255,30,50,200}:Color{160,0,255,200};
            // Spawn echo at current pos
            echoes.push_back({position,facing,0.3f,0.3f,ec});
            VfxSlash(position,facing,ec);
        }
        // Trail particles
        auto& V=Graphics::VFXSystem::GetInstance();
        Color tc=dashAtk.isUltimate?Color{255,20,40,180}:Color{140,0,255,180};
        V.SpawnPremium(position,{0,0},{0,0},0.18f,tc,Fade(tc,0),20.f,0.f,
            Graphics::RenderType::RHOMB,BLEND_ADDITIVE);
    }

    // Scythes (combos 3 & 4)
    for(auto& s:scythes){
        if(!s.active)continue;
        s.progress+=(s.isReturning?1.6f:2.4f)*dt;
        if(s.progress>=1.f){s.active=false;continue;}
        if(s.isReturning){
            Vector2 side={-facing.y,facing.x};
            s.position=Vector2Add(Vector2Lerp(s.startPos,position,s.progress),Vector2Scale(side,sinf(s.progress*PI)*140.f));
        }else{
            s.position=Vector2Lerp(s.startPos,s.targetPos,s.progress);
        }
        s.angle+=38.f*dt;
        Graphics::VFXSystem::GetInstance().SpawnPremium(s.position,{0,0},{0,0},0.13f,{255,0,80,180},{100,0,40,0},22.f,0.f,
            Graphics::RenderType::RHOMB,BLEND_ADDITIVE,Graphics::EasingType::LINEAR,1.f,0,s.angle*RAD2DEG);
    }

    // Blood leaves (R)
    for(auto& l:leaves){
        if(!l.active)continue;
        l.position=Vector2Add(l.position,Vector2Scale(l.velocity,dt));
        float ang=atan2f(l.velocity.y,l.velocity.x)*RAD2DEG;
        Graphics::VFXSystem::GetInstance().SpawnPremium(l.position,{0,0},{0,0},0.15f,{200,0,40,200},{50,0,10,0},
            10.f,3.f,Graphics::RenderType::RHOMB,BLEND_ADDITIVE,Graphics::EasingType::LINEAR,1.f,0,ang);
    }

    // Echoes lifetime
    for(auto& e:echoes)e.lifetime-=dt;
    echoes.erase(std::remove_if(echoes.begin(),echoes.end(),[](const SlashEcho&e){return e.lifetime<=0;}),echoes.end());

    for(auto& pool:pools)if(pool.lifetime>0)pool.lifetime-=dt;

    // Attack phases
    if(state==ReaperState::ATTACKING||state==ReaperState::HEAVY_ATTACK){
        attackPhaseTimer-=dt;
        if(attackPhaseTimer<=0){
            if(state==ReaperState::HEAVY_ATTACK){
                if(attackPhase==AttackPhase::STARTUP){attackPhase=AttackPhase::ATTACK_ACTIVE;attackPhaseTimer=0.2f;hasHit=false;VfxHeavy(position,facing);screenShake=0.3f;}
                else if(attackPhase==AttackPhase::ATTACK_ACTIVE){attackPhase=AttackPhase::RECOVERY;attackPhaseTimer=0.25f;}
                else CancelAttack();
            }else{
                float AT[4]={0.12f,0.12f,0.25f,0.45f},RT[4]={0.15f,0.15f,0.10f,0.25f};
                int step=comboStep>0?comboStep-1:0;
                if(attackPhase==AttackPhase::STARTUP){
                    attackPhase=AttackPhase::ATTACK_ACTIVE;attackPhaseTimer=AT[step];hasHit=false;
                    if(step==0||step==1){VfxSlash(position,facing,{160,0,255,200});}
                    else if(step==2){scythes.push_back({position,position,Vector2Add(position,Vector2Scale(facing,420.f)),0.f,false,true,0.f,32.f*rpg.DamageMultiplierMagical()});Graphics::SpawnSonicBoom(position,100.f);}
                    else{scythes.push_back({position,Vector2Add(position,Vector2Scale(facing,460.f)),position,0.f,true,true,0.f,38.f*rpg.DamageMultiplierMagical()});Graphics::SpawnSonicBoom(position,150.f);}
                }else if(attackPhase==AttackPhase::ATTACK_ACTIVE){attackPhase=AttackPhase::RECOVERY;attackPhaseTimer=RT[step];}
                else CancelAttack();
            }
        }
    }

    if (state == ReaperState::NORMAL && comboStep > 0) {
        comboTimer -= dt;
        if (comboTimer <= 0) comboStep = 0;
    }

    // Heavy charge - track independently of canAct to avoid mid-charge cancel
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) && !dashAtk.active && eStunTimer <= 0 && state != ReaperState::DASHING) {
        isChargingHeavy = true;
        heavyHoldTimer += dt;
        if (GetRandomValue(0, 100) < 25) {
            Vector2 off = {(float)GetRandomValue(-90,90), (float)GetRandomValue(-90,90)};
            Graphics::VFXSystem::GetInstance().SpawnPremium(
                Vector2Add(position, off),
                Vector2Scale(Vector2Normalize(Vector2Negate(off)), 180.f),
                {0,0}, 0.18f, {200,100,255,200}, Fade(WHITE,0), 5.f, 0.f,
                Graphics::RenderType::RHOMB, BLEND_ADDITIVE);
        }
    } else if (isChargingHeavy) {
        if (heavyHoldTimer >= 0.2f && (energy >= 10.f || isAdminMode)) {
            if (!isAdminMode) energy -= 10.f;
            state = ReaperState::HEAVY_ATTACK;
            attackPhase = AttackPhase::STARTUP;
            attackPhaseTimer = 0.18f;
            hasHit = false;
        }
        isChargingHeavy = false;
        heavyHoldTimer = 0.f;
    }

    bool canAct = (state == ReaperState::NORMAL || attackPhase == AttackPhase::RECOVERY)
                  && !dashAtk.active && eStunTimer <= 0 && !isChargingHeavy;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && canAct) {
        // Increment BEFORE starting attack so step index is correct
        if (comboStep >= 4) comboStep = 0;
        comboStep++; // step 1-4; used as comboStep-1 as index
        state = ReaperState::ATTACKING;
        attackPhase = AttackPhase::STARTUP;
        attackPhaseTimer = 0.1f;
        comboTimer = 0.55f;
        hasHit = false;
    }

    if (IsKeyPressed(controls.dash) && CanDash() && canAct) {
        UseDash();
        state = ReaperState::DASHING;
        attackPhaseTimer = 0.42f;
        hitstopTimer = 0.f;
        Vector2 mv = {0,0};
        if (IsKeyDown(KEY_W)) mv.y -= 1;
        if (IsKeyDown(KEY_S)) mv.y += 1;
        if (IsKeyDown(KEY_A)) mv.x -= 1;
        if (IsKeyDown(KEY_D)) mv.x += 1;
        if (Vector2Length(mv) == 0) mv = facing;
        velocity = Vector2Scale(Vector2Normalize(mv), 860.f);
    }
    if(state==ReaperState::DASHING){
        attackPhaseTimer-=dt;velocity=Vector2Scale(velocity,0.93f);
        position=Arena::GetClampedPos(Vector2Add(position,Vector2Scale(velocity,dt)),radius);
        // Ghost trail: same style as other characters
        Graphics::VFXSystem::GetInstance().SpawnGhost(position,{0,0,40,40},0.2f,Fade({180,0,255,255},0.3f),facing.x>0,1.f,{20,20},ResourceManager::texPlayer);
        if(attackPhaseTimer<=0)state=ReaperState::NORMAL;
    } else if (!dashAtk.active && eStunTimer <= 0 && attackPhase == AttackPhase::NONE && !isChargingHeavy) {
        Vector2 mv = {0,0};
        if (IsKeyDown(KEY_W)) mv.y -= 1;
        if (IsKeyDown(KEY_S)) mv.y += 1;
        if (IsKeyDown(KEY_A)) mv.x -= 1;
        if (IsKeyDown(KEY_D)) mv.x += 1;
        if (Vector2Length(mv) > 0)
            position = Arena::GetClampedPos(Vector2Add(position, Vector2Scale(Vector2Normalize(mv), 370.f * dt)), radius);
    }

    if(!dashAtk.active){
        Vector2 aim=Vector2Subtract(targetAim,position);
        if(Vector2Length(aim)>0)facing=Vector2Normalize(aim);
    }
    energy+=(!isAdminMode?8.f:50.f)*dt;if(energy>maxEnergy)energy=maxEnergy;
}

void Reaper::Draw(){
    // Blood pools
    for(const auto& p:pools)if(p.lifetime>0)DrawBloodPool(p.position,fminf(1.f,p.lifetime/30.f));

    // Echoes from dash slashes (ghost imprint on ground)
    for(const auto& e:echoes){
        float t=e.lifetime/e.maxLifetime;
        float ang=atan2f(e.dir.y*2.f,e.dir.x)*RAD2DEG;
        DrawIsoSector(e.position,130.f*t,ang-60.f,ang+60.f,Fade(e.color,t*0.6f));
    }

    // Scythe projectiles
    for(const auto& s:scythes)if(s.active){
        DrawScythe(s.position,s.angle,42.f,{255,30,80,255});
        DrawEllipseLines((int)s.position.x,(int)s.position.y,38,19,Fade(RED,0.7f));
    }

    // Blood leaves (aletas de tiburon)
    for(const auto& l:leaves)if(l.active){
        float ang=atan2f(l.velocity.y,l.velocity.x);
        Vector2 tip={l.position.x+cosf(ang)*24.f,l.position.y+sinf(ang)*24.f};
        Vector2 lp={l.position.x+cosf(ang+2.5f)*14.f,l.position.y+sinf(ang+2.5f)*14.f};
        Vector2 rp={l.position.x+cosf(ang-2.5f)*14.f,l.position.y+sinf(ang-2.5f)*14.f};
        DrawTriangle(l.position,lp,tip,{200,0,40,255});
        DrawTriangle(l.position,tip,rp,{200,0,40,255});
        DrawTriangle(l.position,tip,lp,{200,0,40,255});
        DrawTriangle(l.position,rp,tip,{200,0,40,255});
        DrawLineEx(tip,lp,2.f,WHITE);DrawLineEx(tip,rp,2.f,WHITE);
        DrawCircleLines((int)l.position.x,(int)l.position.y,11.f,Fade(RED,0.75f));
    }

    // Player body
    DrawCircleV({position.x,position.y-22},radius,hitFlashTimer>0?WHITE:color);
    DrawCircleLines(position.x,position.y-22,radius+2,{220,100,255,255});
    DrawCircleV(Vector2Add({position.x,position.y-22},Vector2Scale(facing,radius+8)),4,GOLD);

    // Hitboxes for combos 1 & 2
    if(state==ReaperState::ATTACKING&&attackPhase==AttackPhase::ATTACK_ACTIVE){
        int step=comboStep>0?comboStep-1:0;
        if(step==0||step==1){
            float p=fmaxf(0.01f,1.f-(attackPhaseTimer/0.12f));
            float ang=atan2f(facing.y*2.f,facing.x)*RAD2DEG;
            float sv=step==0?-70.f:70.f, sw=step==0?140.f:-140.f;
            DrawIsoSector(position,140.f,ang+sv,ang+sv+sw*p,Fade(RED,0.45f));
            DrawIsoSector(position,140.f*p,ang+sv,ang+sv+sw*p,Fade(WHITE,0.7f));
        }
    }
    // Heavy hitbox
    if(state==ReaperState::HEAVY_ATTACK&&attackPhase==AttackPhase::ATTACK_ACTIVE){
        float p=fmaxf(0.01f,1.f-(attackPhaseTimer/0.20f));
        float ang=atan2f(facing.y*2.f,facing.x)*RAD2DEG;
        DrawIsoSector(position,130.f,ang-70.f,ang-70.f+140.f*p,Fade({255,0,50,255},0.55f));
    }
    // DashAttack hitbox ring
    if(dashAtk.active){
        DrawEllipseLines((int)position.x,(int)position.y,160,80,Fade(RED,0.8f));
        DrawEllipse((int)position.x,(int)position.y,160,80,Fade(RED,0.12f));
    }
}

void Reaper::Reset(Vector2 pos){
    position=pos;hp=maxHp;energy=maxEnergy;velocity={0,0};
    CancelAttack();pools.clear();leaves.clear();scythes.clear();echoes.clear();
    qCooldown=eCooldown=ultCooldown=0;
}

void Reaper::HandleSkills(Boss& boss){
    bool can=state==ReaperState::NORMAL&&!dashAtk.active&&eStunTimer<=0&&!isChargingHeavy;

    // Q: Dash de Cortes
    if(IsKeyPressed(controls.boomerang)&&can&&qCooldown<=0){
        qCooldown=7.f;
        Vector2 dir=Vector2Subtract(targetAim,position);
        if(Vector2Length(dir)<1.f)dir=facing;else dir=Vector2Normalize(dir);
        StartDashAttack(Vector2Add(position,Vector2Scale(dir,480.f)),false);
    }

    // E: Sacrificio (espinas de sangre)
    if(IsKeyPressed(controls.berserker)&&can&&eCooldown<=0){
        eCooldown=3.5f;
        if(!isAdminMode)hp-=15.f;
        eStunTimer=0.35f;
        pools.push_back({position,30.f});
    }

    // R: Ejecucion (igual que Q pero mas)
    if(IsKeyPressed(controls.ultimate)&&can&&ultCooldown<=0){
        ultCooldown=18.f;
        Vector2 dir=Vector2Subtract(targetAim,position);
        if(Vector2Length(dir)<1.f)dir=facing;else dir=Vector2Normalize(dir);
        StartDashAttack(Vector2Add(position,Vector2Scale(dir,650.f)),true);
    }
}

void Reaper::CheckCollisions(Boss& boss){
    if(boss.isDead||boss.isDying)return;

    // Basicos 1 & 2 - NO incrementar comboStep aqui, ya se incrementa al clickar
    if (state == ReaperState::ATTACKING && attackPhase == AttackPhase::ATTACK_ACTIVE && !hasHit) {
        int step = comboStep > 0 ? comboStep - 1 : 0;
        if (step == 0 || step == 1) {
            float p = fmaxf(0.01f, 1.f - (attackPhaseTimer / 0.12f));
            float sv = step == 0 ? -70.f : 70.f, sw = step == 0 ? 140.f : -140.f;
            if (CombatUtils::CheckProgressiveSweep(position, facing, boss.position, boss.radius, 140.f, sv, sw, 1.f, p)) {
                hasHit = true;
                boss.TakeDamage(25.f * rpg.DamageMultiplierMixed(), 2.f, Vector2Scale(facing, 60.f));
                Graphics::SpawnStyledBlood(boss.position, facing);
            }
        }
    }

    // Scythes (3 & 4) - no incrementar comboStep
    for (auto& s : scythes) {
        if (!s.active) continue;
        if (hitCooldownTimer <= 0 && CombatUtils::GetIsoDistance(s.position, boss.position) <= boss.radius + 40.f) {
            hitCooldownTimer = 0.38f;
            boss.TakeDamage(s.damage, 2.f, Vector2Scale(facing, 50.f));
            Graphics::SpawnStyledBlood(boss.position, facing);
        }
    }

    // Heavy
    if(state==ReaperState::HEAVY_ATTACK&&attackPhase==AttackPhase::ATTACK_ACTIVE&&!hasHit){
        float p=1.f-(attackPhaseTimer/0.20f);
        if(CombatUtils::CheckProgressiveSweep(position,facing,boss.position,boss.radius,130.f,-70.f,140.f,1.f,p)){
            hasHit=true;boss.TakeDamage(55.f*rpg.DamageMultiplierMagical(),4.f,Vector2Scale(facing,120.f));
            ApplyBleed(boss);Graphics::SpawnStyledBlood(boss.position,facing);
        }
    }

    // DashAttack hits (multiple per dash using hitCooldownTimer)
    if(dashAtk.active&&hitCooldownTimer<=0&&CombatUtils::GetIsoDistance(position,boss.position)<=boss.radius+160.f){
        hitCooldownTimer=0.09f;
        float dmg=dashAtk.isUltimate?30.f:22.f;
        dmg*=rpg.DamageMultiplierMagical();
        boss.TakeDamage(dmg,0.4f,Vector2Scale(facing,30.f));
        Graphics::SpawnStyledBlood(boss.position,facing);
        if(boss.isBleeding)hp=fminf(hp+dmg*0.2f,maxHp);
        // On first R hit: launch leaves from pools
        if(dashAtk.isUltimate&&!dashAtk.hitEnemy){
            dashAtk.hitEnemy=true;
            ApplyBleed(boss);screenShake=1.f;
            for(auto& pl:pools)if(pl.lifetime>0){
                Vector2 d=Vector2Normalize(Vector2Subtract(boss.position,pl.position));
                leaves.push_back({pl.position,Vector2Scale(d,920.f),25.f*rpg.DamageMultiplierMagical(),true});
                Graphics::SpawnSonicBoom(pl.position,60.f);pl.lifetime=0;
            }
        }
    }

    // E explosion
    if(eExplosionReady){
        eExplosionReady=false;
        if(CombatUtils::GetIsoDistance(position,boss.position)<=boss.radius+210.f){
            boss.TakeDamage(28.f*rpg.DamageMultiplierMagical(),3.f,Vector2Normalize(Vector2Subtract(boss.position,position)));
            ApplyBleed(boss);
        }
    }

    // Blood leaves
    for(auto& l:leaves){
        if(!l.active)continue;
        if(CombatUtils::GetIsoDistance(l.position,boss.position)<=boss.radius+12.f){
            l.active=false;boss.TakeDamage(l.damage,1.f,{0,0});
            Graphics::SpawnStyledBlood(boss.position,Vector2Normalize(l.velocity));
        }
    }
}

std::vector<AbilityInfo> Reaper::GetAbilities() const {
    return {
        {"Dash",dashCooldown1,dashMaxCD,(float)dashCharges,dashCharges>0,{200,200,200,255},{}},
        {"Q: Cosecha",qCooldown,7.f,0,qCooldown<=0,{160,0,255,255},{}},
        {"E: Sacrificio",eCooldown,3.5f,0,eCooldown<=0,{200,0,60,255},{}},
        {"R: Ejecucion",ultCooldown,18.f,0,ultCooldown<=0,{220,0,30,255},{}},
    };
}
