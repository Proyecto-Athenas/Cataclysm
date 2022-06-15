
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "grimbatol.h"

enum Texts
{
    SAY_ENGAGE                  = 0,
    SAY_SHAD_GALE               = 1,
    SAY_GUARDIAN                = 2,
    SAY_SLAY                    = 3,
    SAY_DEATH                   = 4,
    EMOTE_SHADOW_GALE           = 5,
    EMOTE_FACELESS              = 6,
    EMOTE_SHIELD_OF_NIGHTMARES  = 7,
};

enum Spells
{
    SPELL_ENFEEBLING_BLOW           = 75789,
    SPELL_BINDING_SHADOWS           = 79466,
    SPELL_BINDING_SHADOWS_AURA      = 75861,
    SPELL_SIPHON_ESSENSE            = 75755,
    SPELL_UMBRAL_MENDING            = 75763,
    SPELL_SHADOW_GALE_SPEED_TRIGGER = 75675,
    SPELL_SHADOW_GALE_SPEED         = 75694,
    SPELL_SHADOW_GALE               = 75664,
    SPELL_SHADOW_GALE_DMG           = 75692,
    SPELL_TWILIGHT_CORRUPTION       = 75520,
    SPELL_TWILIGHT_CORRUPTION_DMG   = 75566,
    SPELL_SUMMON_TWILIGHT_HATCHLING = 91058,
    SPELL_SPAWN_FACELESS            = 75704,
    SPELL_TWILIGHT_PORTAL_VISUAL    = 95716,
    SPELL_TWILIGHT_BLAST_TRIGGER    = 76192,
    SPELL_TWILIGHT_BLAST_DMG        = 76194,
    SPELL_SHIELD_OF_NIGHTMARES      = 75809,
};

enum Events
{
    EVENT_ENFEEBLING_BLOW = 1,
    EVENT_BINDING_SHADOWS,
    EVENT_ADDS,
    EVENT_SIPHON_ESSENSE,
    EVENT_UMBRAL_MENDING,
    EVENT_REMOVE_TWILIGHT_PORTAL,
    EVENT_SHADOW_GALE,
    EVENT_TWILIGHT_CORRUPTION,
    EVENT_CORRUPTOR_MOVE,
    EVENT_SHIELD_OF_NIGHTMARES,
};

enum MiscTypes
{
    POINT_SHADOWGALE = 1001,
    POINT_EGG        = 1002,

    // Misc
    ACTION_INIT_COMBAT,
};

class boss_erudax : public CreatureScript
{
    public:
        boss_erudax() : CreatureScript("boss_erudax") { }

        struct boss_erudaxAI : public BossAI
        {
            boss_erudaxAI(Creature* creature) : BossAI(creature, DATA_ERUDAX)
            {
                me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_ROOT, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FREEZE, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SAPPED, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM, true);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED, true);
                me->setActive(true);
            }

            Creature* FacelessPortalStalker;
            Creature* ShadowGaleTrigger;
            uint64 targetGUID;
            uint32 allowAchiev;

            void Reset() override
            {
                _Reset();

                if (instance)
                    instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

                summons.DespawnAll();
                targetGUID = 0;

                for (uint8 i = 0; i < 13; i++)
                    me->SummonCreature(NPC_ALEXSTRASZA_EGG, eggPos[i]);

                allowAchiev = 1;
            }

            void JustSummoned(Creature* summon) override
            {
                summons.Summon(summon);

                if (summon->GetEntry() == NPC_FACELESS_CORRUPTOR ||
                    summon->GetEntry() == NPC_FACELESS_CORRUPTOR_H)
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 0.0f, true))
                        summon->AI()->AttackStart(target);
            }

            uint32 GetData(uint32 type) const override
            {
                if (type == DATA_BREAK_THE_EGGS)
                    return allowAchiev;

                return 0;
            }

            void SetData(uint32 type, uint32 data) override
            {
                if (type == DATA_BREAK_THE_EGGS)
                    allowAchiev = data;
            }

            void KilledUnit(Unit* victim)
            {
                if (victim->GetTypeId() == TYPEID_PLAYER)
                    Talk(SAY_SLAY);
            }

            void SummonedCreatureDespawn(Creature* summon) override
            {
                summons.Despawn(summon);
            }

            void EnterCombat(Unit* /*who*/) override
            {
                _EnterCombat();

                if (instance)
                    instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, me);

                Talk(SAY_ENGAGE);
                FacelessPortalStalker = me->SummonCreature(NPC_FACELESS_PORTAL_STALKER, erudaxportalPos, TEMPSUMMON_MANUAL_DESPAWN);
                events.ScheduleEvent(EVENT_SHADOW_GALE, urand(25000, 26000));
                events.ScheduleEvent(EVENT_ENFEEBLING_BLOW, urand(4000, 6000));
                events.ScheduleEvent(EVENT_BINDING_SHADOWS, urand(9000, 11000));
            }

            void JustDied(Unit* /*killer*/) override
            {
                _JustDied();

                if (instance)
                    instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, me);

                Talk(SAY_DEATH);
                summons.DespawnAll();
            }

            void UpdateAI(uint32 diff) override
            {
                scheduler.Update(diff);

                if (!UpdateVictim())
                    return;

                if (me->GetDistance(me->GetHomePosition()) > 100.0f)
                {
                    EnterEvadeMode();
                    return;
                }

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_SHADOW_GALE:
                            Talk(EMOTE_SHADOW_GALE);
                            Talk(SAY_SHAD_GALE);
                            if (ShadowGaleTrigger = me->SummonCreature(NPC_SHADOW_GALE_STALKER, shadowgalePos[urand(0, 2)]))
                                ShadowGaleTrigger->CastSpell(ShadowGaleTrigger, SPELL_SHADOW_GALE_SPEED_TRIGGER, false);

                            DoCast(me, SPELL_SHADOW_GALE);
                            events.ScheduleEvent(EVENT_ADDS, 16500);
                            break;
                        case EVENT_REMOVE_TWILIGHT_PORTAL:
                            if (FacelessPortalStalker)
                                FacelessPortalStalker->RemoveAurasDueToSpell(SPELL_TWILIGHT_PORTAL_VISUAL);
                            break;
                        case EVENT_ADDS:
                            if (ShadowGaleTrigger)
                                ShadowGaleTrigger->DespawnOrUnsummon();

                            Talk(EMOTE_FACELESS);
                            Talk(SAY_GUARDIAN);
                            if (FacelessPortalStalker)
                            {
                                FacelessPortalStalker->CastSpell(FacelessPortalStalker, SPELL_TWILIGHT_PORTAL_VISUAL, true);
                                me->CastSpell(FacelessPortalStalker, SPELL_SPAWN_FACELESS, true);
                            }

                            if (IsHeroic())
                                events.ScheduleEvent(EVENT_SHIELD_OF_NIGHTMARES, urand(4000, 7000));

                            events.ScheduleEvent(EVENT_REMOVE_TWILIGHT_PORTAL, 7000);
                            events.ScheduleEvent(EVENT_SHADOW_GALE, urand(40000, 44000));
                            break;
                        case EVENT_ENFEEBLING_BLOW:
                            if (Unit* target = me->getVictim())
                                DoCast(target, SPELL_ENFEEBLING_BLOW);

                            events.ScheduleEvent(EVENT_ENFEEBLING_BLOW, urand(20000, 30000));
                            break;
                        case EVENT_BINDING_SHADOWS:
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, NonTankSpecTargetSelector()))
                            {
                                if (Unit* target = me->getVictim())
                                    targetGUID = target->GetGUID();

                                me->PrepareChanneledCast(me->GetOrientation());

                                uint64 bindingGUID = target->GetGUID();
                                scheduler
                                    .Schedule(Milliseconds(500), [this, bindingGUID](TaskContext context)
                                {
                                    if (Unit* tempTarget = ObjectAccessor::GetUnit(*me, bindingGUID))
                                        DoCast(tempTarget, SPELL_BINDING_SHADOWS);
                                });

                                scheduler
                                    .Schedule(Milliseconds(2500), [this](TaskContext context)
                                {
                                    me->RemoveChanneledCast(targetGUID);
                                });
                            }
                            else if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 200.0f, true))
                            {
                                if (Unit* target = me->getVictim())
                                    targetGUID = target->GetGUID();

                                me->PrepareChanneledCast(me->GetOrientation());

                                uint64 bindingGUID = target->GetGUID();
                                scheduler
                                    .Schedule(Milliseconds(500), [this, bindingGUID](TaskContext context)
                                {
                                    if (Unit* tempTarget = ObjectAccessor::GetUnit(*me, bindingGUID))
                                        DoCast(tempTarget, SPELL_BINDING_SHADOWS);
                                });

                                scheduler
                                    .Schedule(Milliseconds(2500), [this](TaskContext context)
                                {
                                    me->RemoveChanneledCast(targetGUID);
                                });
                            }

                            events.ScheduleEvent(EVENT_BINDING_SHADOWS, urand(20000, 30000));
                            break;
                        case EVENT_SHIELD_OF_NIGHTMARES:
                            Talk(EMOTE_SHIELD_OF_NIGHTMARES);
                            DoCast(me, SPELL_SHIELD_OF_NIGHTMARES);
                            break;
                    }
                }
                DoMeleeAttackIfReady();
            }
        };

        CreatureAI* GetAI(Creature* creature) const override
        {
            return GetInstanceAI<boss_erudaxAI>(creature);
        }
};

// Faceless Corruptor 40600, 48844
struct npc_erudax_faceless_corruptor : public ScriptedAI
{
    npc_erudax_faceless_corruptor(Creature* creature) : ScriptedAI(creature)
    {
        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_ROOT, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FREEZE, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SAPPED, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM, true);
        me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED, true);
        instance = creature->GetInstanceScript();
    }

    InstanceScript* instance;
    TaskScheduler scheduler;
    EventMap events;

    void IsSummonedBy(Unit* summoner) override
    {
        me->SetInCombatWithZone();
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();

        me->GetMotionMaster()->MovePoint(0, eggsSpawnPath[me->GetEntry() == NPC_FACELESS_CORRUPTOR_H ? 0 : 1]);

        scheduler
            .Schedule(Milliseconds(me->GetSplineDuration()), [this](TaskContext context)
        {
            DoCast(me, SPELL_TWILIGHT_CORRUPTION);

            if (Creature* _erudax = ObjectAccessor::GetCreature(*me, instance ? instance->GetData64(DATA_ERUDAX) : 0))
                return _erudax->AI()->SetData(DATA_BREAK_THE_EGGS, 0);
        });
    }

    void DoAction(int32 actionId) override
    {
        if (actionId == ACTION_INIT_COMBAT)
        {
            me->RemoveChanneledCast();

            events.ScheduleEvent(EVENT_UMBRAL_MENDING, urand(15000, 20000));
            events.ScheduleEvent(EVENT_SIPHON_ESSENSE, urand(5000, 7000));
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->DespawnOrUnsummon(1000);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!instance || !UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_UMBRAL_MENDING:
                    if (Creature* erudax = Unit::GetCreature(*me, instance->GetData64(DATA_ERUDAX)))
                        DoCast(erudax, SPELL_UMBRAL_MENDING);

                    events.ScheduleEvent(EVENT_UMBRAL_MENDING, urand(15000, 20000));
                    break;
                case EVENT_SIPHON_ESSENSE:
                    if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 0.0f, true))
                        DoCast(target, SPELL_SIPHON_ESSENSE);

                    events.ScheduleEvent(EVENT_SIPHON_ESSENSE, urand(7000, 10000));
                    break;
            }
        }

        DoMeleeAttackIfReady();
    }
};

// Alexstraza`s Egg 40486
struct npc_alexstrasza_egg : public ScriptedAI
{
    npc_alexstrasza_egg(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
        instance = creature->GetInstanceScript();
        SetCombatMovement(false);
    }

    void Reset() override
    {
        me->setRegeneratingHealth(false);
    }

    InstanceScript* instance;

    void JustDied(Unit* /*killer*/) override
    {
        DoCast(me, SPELL_SUMMON_TWILIGHT_HATCHLING, true);
    }

    void JustSummoned(Creature* summon) override
    {
        if (!instance)
            return;

        if (summon->GetEntry() == NPC_TWILIGHT_HATCHLING)
        {
            if (Creature* _erudax = ObjectAccessor::GetCreature(*me, instance->GetData64(DATA_ERUDAX)))
            {
                if (Unit* target = _erudax->AI()->SelectTarget(SELECT_TARGET_RANDOM, 0, 0.0f, true))
                {
                    summon->AI()->AttackStart(target);
                    summon->GetMotionMaster()->MoveChase(target);
                }
            }
        }
    }
};

// Shadow Gale Stalker 44314
struct npc_shadow_gale_stalker : public ScriptedAI
{
    npc_shadow_gale_stalker(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
        SetCombatMovement(false);
    }

    void Reset() override { }
};

// Twilight Hatchling 39388
struct npc_erudax_twilight_hatchling : public ScriptedAI
{
    npc_erudax_twilight_hatchling(Creature* creature) : ScriptedAI(creature)
    {
        SetCombatMovement(false);
        instance = creature->GetInstanceScript();
    }

    InstanceScript* instance;

    void Reset() override
    {
        me->SetCanFly(true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->DespawnOrUnsummon();
    }

    void IsSummonedBy(Unit* /*owner*/) override
    {
        DoCast(me, SPELL_TWILIGHT_BLAST_TRIGGER);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!instance)
            return;

        if (instance->GetBossState(DATA_ERUDAX) != IN_PROGRESS)
            me->DespawnOrUnsummon();
    }
};

// Shadow Gale 75692
class spell_erudax_shadow_gale : public SpellScript
{
    PrepareSpellScript(spell_erudax_shadow_gale);

    void HandleEffectHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            if (target->FindNearestCreature(NPC_SHADOW_GALE_STALKER, 5.0f))
                PreventHitDamage();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_erudax_shadow_gale::HandleEffectHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Shield of Nightmares 75809
class spell_erudax_shield_of_nightmares : public SpellScript
{
    PrepareSpellScript(spell_erudax_shield_of_nightmares);

    void SelectTargets(std::list<WorldObject*>&targets)
    {
        targets.remove_if([=](WorldObject* target) { return target->GetEntry() != NPC_FACELESS_CORRUPTOR; });

        if (targets.size() > 1)
            Trinity::Containers::RandomResizeList(targets, 1);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_erudax_shield_of_nightmares::SelectTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

class twilightCorruptionOrderPred
{
    public:
        twilightCorruptionOrderPred(const WorldObject* object, bool ascending = true) : m_object(object), m_ascending(ascending) {}
        bool operator() (const WorldObject* a, const WorldObject* b) const
        {
            return m_ascending ? a->GetDistance(m_object) < b->GetDistance(m_object) :
                a->GetDistance(m_object) > b->GetDistance(m_object);
        }

    private:
        const WorldObject* m_object;
        const bool m_ascending;
};

// Twilight Corruption 75520, 91049
class spell_erudax_twilight_corruption : public SpellScript
{
    PrepareSpellScript(spell_erudax_twilight_corruption);

    void SelectTargets(std::list<WorldObject*>&targets)
    {
        targets.remove_if([=](WorldObject* target) { return target->GetEntry() != NPC_ALEXSTRASZA_EGG; });

        if (targets.empty())
            return;

        targets.sort(twilightCorruptionOrderPred(GetCaster(), true));

        if (targets.size() > 1)
            targets.resize(1);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_erudax_twilight_corruption::SelectTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

// Twilight Corruption 75520, 91049
class spell_erudax_twilight_corruption_aura : public AuraScript
{
    PrepareAuraScript(spell_erudax_twilight_corruption_aura);

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetCaster() && GetCaster()->ToCreature())
            GetCaster()->ToCreature()->AI()->DoAction(ACTION_INIT_COMBAT);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_erudax_twilight_corruption_aura::OnRemove, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// Umbral Mending 75763, 91040
class spell_erudax_umbral_mending : public SpellScript
{
    PrepareSpellScript(spell_erudax_umbral_mending);

    void SelectTargets(std::list<WorldObject*>&targets)
    {
        targets.remove_if([=](WorldObject* target) { return target->GetEntry() != BOSS_ERUDAX; });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_erudax_umbral_mending::SelectTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

class achievement_break_the_eggs : public AchievementCriteriaScript
{
    public:
        achievement_break_the_eggs() : AchievementCriteriaScript("achievement_break_the_eggs") { }

        bool OnCheck(Player* /*source*/, Unit* target) override
        {
            if (!target)
                return false;

            if (Creature* _erudax = ObjectAccessor::GetCreature(*target, target->GetInstanceScript() ? target->GetInstanceScript()->GetData64(DATA_ERUDAX) : 0))
                return _erudax->AI()->GetData(DATA_BREAK_THE_EGGS);

            return false;
        }
};

void AddSC_boss_erudax()
{
    new boss_erudax();
    new CreatureScriptLoaderEx<npc_erudax_faceless_corruptor>("npc_erudax_faceless_corruptor");
    new CreatureScriptLoaderEx<npc_alexstrasza_egg>("npc_alexstrasza_egg");
    new CreatureScriptLoaderEx<npc_shadow_gale_stalker>("npc_shadow_gale_stalker");
    new CreatureScriptLoaderEx<npc_erudax_twilight_hatchling>("npc_erudax_twilight_hatchling");
    new SpellScriptLoaderEx<spell_erudax_shadow_gale>("spell_erudax_shadow_gale");
    new SpellScriptLoaderEx<spell_erudax_shield_of_nightmares>("spell_erudax_shield_of_nightmares");
    new SpellScriptLoaderEx<spell_erudax_twilight_corruption>("spell_erudax_twilight_corruption");
    new AuraScriptLoaderEx<spell_erudax_twilight_corruption_aura>("spell_erudax_twilight_corruption_aura");
    new SpellScriptLoaderEx<spell_erudax_umbral_mending>("spell_erudax_umbral_mending");
    new achievement_break_the_eggs();
}
