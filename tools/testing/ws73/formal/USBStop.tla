----------------------------- MODULE USBStop -----------------------------
\* SPDX-License-Identifier: GPL-2.0-only
\* Finite safety abstraction: see USBStop.README for source mapping/limits.
EXTENDS TLC
CONSTANT DrainBackendFirst
VARIABLES actor, failed, pc, stop
vars == <<actor, failed, pc, stop>>
Actors == {"rx", "irq", "init", "backend"}
Receivers == {"rx", "irq"}
Inactive(a) == actor[a] \in {"idle", "done"}
ReceiversJoined == \A a \in Receivers : Inactive(a)
Init == /\ actor = [a \in Actors |-> IF a = "init" THEN "active" ELSE "idle"]
        /\ failed = FALSE
        /\ pc = "live"
        /\ stop = "idle"

\* Admission and the disconnect/fault gate use the same parent spinlock.
Begin(a) == /\ a \in {"rx", "irq", "backend"}
            /\ actor[a] = "idle" /\ pc = "live" /\ ~failed
            /\ actor' = [actor EXCEPT ![a] = IF a \in Receivers
                                           THEN "submitted" ELSE "active"]
            /\ UNCHANGED <<failed, pc, stop>>
\* Giveback unanchors before calling the completion, but anchor suspend_wakeups
\* remains held. The join below must wait for the callback to finish as well.
Giveback(a) == /\ a \in Receivers /\ actor[a] = "submitted"
               /\ actor' = [actor EXCEPT ![a] = "active"]
               /\ UNCHANGED <<failed, pc, stop>>
\* ws73_usb_fail sets first under ws->lock, then drops it before abort/queue.
ClaimFault(a) == /\ actor[a] = "active" /\ pc = "live" /\ ~failed
                 /\ failed' = TRUE
                 /\ actor' = [actor EXCEPT ![a] = "claimed"]
                 /\ UNCHANGED <<pc, stop>>
QueueStop(a) == /\ actor[a] = "claimed"
               /\ actor' = [actor EXCEPT ![a] = "enqueued"]
               /\ stop' = "queued"
               /\ UNCHANGED <<failed, pc>>
Return(a) == /\ actor[a] \in {"active", "enqueued"}
             /\ actor' = [actor EXCEPT ![a] = "done"]
             /\ UNCHANGED <<failed, pc, stop>>
RunStop == /\ stop = "queued" /\ stop' = "running"
           /\ UNCHANGED <<actor, failed, pc>>
EndStop == /\ stop = "running" /\ ReceiversJoined
           /\ stop' = "idle" /\ UNCHANGED <<actor, failed, pc>>

Close == /\ pc = "live" /\ pc' = "anchor"
         /\ UNCHANGED <<actor, failed, stop>>
JoinAnchor == /\ pc = "anchor" /\ ReceiversJoined /\ pc' = "rx"
              /\ UNCHANGED <<actor, failed, stop>>
JoinRx == /\ pc = "rx" /\ Inactive("rx") /\ pc' = "irq"
          /\ UNCHANGED <<actor, failed, stop>>
JoinIrq == /\ pc = "irq" /\ Inactive("irq") /\ pc' = "init"
           /\ UNCHANGED <<actor, failed, stop>>
JoinInit == /\ pc = "init" /\ Inactive("init") /\ pc' = "before_child"
            /\ UNCHANGED <<actor, failed, stop>>
\* The FALSE branch is the historical order, not a production option.
BeforeChild == /\ pc = "before_child"
               /\ IF DrainBackendFirst
                     THEN UNCHANGED stop
                     ELSE /\ stop # "running" /\ stop' = "idle"
               /\ pc' = "child" /\ UNCHANGED <<actor, failed>>
RemoveChild == /\ pc = "child" /\ Inactive("backend")
               /\ pc' = "after_child" /\ UNCHANGED <<actor, failed, stop>>
AfterChild == /\ pc = "after_child"
              /\ IF DrainBackendFirst
                    THEN /\ stop # "running" /\ stop' = "idle"
                    ELSE UNCHANGED stop
              /\ pc' = "free" /\ UNCHANGED <<actor, failed>>
Free == /\ pc = "free" /\ pc' = "dead"
        /\ UNCHANGED <<actor, failed, stop>>
\* Explicit terminal stutter keeps TLC's deadlock check enabled elsewhere.
Terminal == /\ pc = "dead" /\ UNCHANGED vars
Next == \/ (\E a \in Actors : Begin(a) \/ Giveback(a) \/ ClaimFault(a)
                               \/ QueueStop(a) \/ Return(a))
        \/ RunStop \/ EndStop \/ Close \/ JoinAnchor \/ JoinRx \/ JoinIrq
        \/ JoinInit \/ BeforeChild \/ RemoveChild \/ AfterChild \/ Free \/ Terminal

TypeOK == /\ actor \in [Actors -> {"idle", "submitted", "active", "claimed",
                                 "enqueued", "done"}]
          /\ failed \in BOOLEAN
          /\ pc \in {"live", "anchor", "rx", "irq", "init", "before_child",
                      "child", "after_child", "free", "dead"}
          /\ stop \in {"idle", "queued", "running"}
NoUseAfterFree == pc = "dead" =>
                  (stop = "idle" /\ (\A a \in Actors : Inactive(a)))
=============================================================================
