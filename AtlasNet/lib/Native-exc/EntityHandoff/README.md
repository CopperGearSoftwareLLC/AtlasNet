> This document captures protocol inspiration, edge cases, and iteration notes for the `EntityHandoff` module.

*** notes for protocol inspiration

entity handoff notes:
shards only know what it owns, is passing on, and ghosts
if passing fails for any reason, move passing entity into own again

handoff itself requires servers agreeing on a specific time to switch authority
while switch is happening, new server takes incoming entity and tracks is as ghost
after agreed time, servers swap authority


Handoff edge case handling v2: Border crossing creates paradoxes 

Look at # 3 and bottom section 

 

Changes: simplifying handoff 

Proxy cannot drop authority packets from server A, only route client packets (keep proxy dumb).  

If the proxy drops the old server’s last few snapshots right at handoff,  

the client can lose the final authoritative baseline it needs to reconcile cleanly 

 

Server A will still drop old client packets after authority has changed 

 

1) World structure (how zones actually exist) 

The continent is partitioned into simulation regions: 

[Server 1] | [Server 2] | [Server 3] 
    base        field        base 
 

But the player never loads. 

Instead: 

Each entity has 

owner server      (authoritative simulator) 
interest servers  (shadow simulators) 
 

So near borders: 

Player standing near border: 
 
Server A: authoritative 
Server B: predictive mirror 
 

This overlap is CRITICAL. 

 

 

2) Crossing a border (handoff process) 

Phase 1 — Pre-handoff (ghosting) 

When you approach a boundary: 

Server B starts simulating you BEFORE transfer 

time t0: 
A authoritative 
B spectator copy 
 
time t1: 
A authoritative 
B predictive sim (shadow authority) 
 

Now both servers simulate you. 

No ownership switch yet. 

 

 

Phase 2 — Authority transfer 

The handoff is NOT position-based. 

It is time-stamped event-based: 

transferTick = agreed simulation tick 
 

At that tick: 

Server A → relinquishes authority 
Server B → becomes authoritative 
 

No gap exists because B already had a full state history. 

This prevents teleport/rubberband. 

 

 

3) The important part: Combat across borders 

The PS2 uses (and most MMOs don’t) is: 

Actions are owned by the server that created the action, not the player’s current server. 

This avoids paradoxes. 

 

 

Shooting example 

Player on Server A shoots someone on Server B. 

Shooter authority: Server A 
Target authority: Server B 
 

Flow: 

1) A validates shot 
2) A sends "damage event" to B 
3) B applies damage 
4) B decides death 
 

So: 

hit detection = shooter server 

health/death = victim server 

No voting. No rollback. Deterministic. 

 

 

Grenades / projectiles 

Once spawned: 

The projectile owns itself 

It stays simulated on the spawn server even if it crosses borders. 

That’s why explosions still hit players across partitions (historically visible in large battles). (Reddit) 

 

 

4) Player death during transfer (your main question) 

The dangerous moment: 

Player crossing boundary 
+ 
Bullet fired before handoff 
+ 
Death occurs after handoff 
 

This is where naive systems break. 

 

 

PlanetSide 2 solution: “Authority at event time” 

The kill is resolved by the server that owned the player at the moment damage was applied, not current ownership. 

So the pipeline: 

tick 100: player on Server A 
tick 101: shot fired 
tick 102: handoff to Server B 
tick 103: damage processed → death 
 

Result: 

Server A still decides kill validity 

Server B only applies state change 

So no double deaths, no invulnerability. 

 

 

5) Why this works 

They separate three things: 

Concept 

Owner 

input validation 

shooter server 

physics 

object owner 

health state 

victim server 

authority transfer 

timeline based 

This removes circular dependency. 

 

 

6) What would break without it 

If ownership switched instantly: 

A says alive 
B says dead 
client predicts alive 
result = desync 
 

Instead they rely on: 

causal ordering (event timestamping) 

Not positional authority. 

 

 

Key takeaway architecture pattern 

PlanetSide 2 is effectively: 

Distributed lockstep with localized authority domains and timestamped events. 

Not: 

lockstep RTS 

simple server zones 

nor pure client authority 

It’s closer to a distributed physics graph. 

 

 

How to replicate this (simplified rule set) 

Use these invariants: 

Server that creates an action validates it 

Server that owns the target applies it 

Ownership changes only on scheduled ticks 

Events reference simulation time, not server 

Transfer never invalidates past events 

If you follow those 5 rules → border deaths become deterministic. 