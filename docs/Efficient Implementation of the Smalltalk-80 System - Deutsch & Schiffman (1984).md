Efficient lmpl?met:tation of the Smalltalk-80 S sty_.~_qL"n
I.. Peter I)cutsch
Xerox PARC, Software Concepts Group
Allan M. Schiffman
Fairchild I.ahoratory hw Artificial Intelligence Research
machine instruction set, similar to the Pascal P-system Ammann
ABS'I'I{ACr  75  Ammann  77.  One unusual  feature of the Smalltalk-80 v-
|     |     |     |     |     |     | machine  | is  that  it  makes  | runtime  | state  such  | as  procedure  |     |
| --- | --- | --- | --- | --- | --- | -------- | -------------------- | -------- | ------------ | -------------- | --- |
The Smalltalk-80* programming language includes dynamic
|     |     |     |     |     |     | activations visible to tile programmer as data objects.  |     |     |     |     | This is  |
| --- | --- | --- | --- | --- | --- | -------------------------------------------------------- | --- | --- | --- | --- | -------- |
storage  allocation,  fuU  upward  limargs,  and  universally  similar to tile "spaghetti stack" model of Interlisp XSIS  83l, but
polymorphic procedures;  file Smalllalk-80 programming system  more  straightforward:  Intcrlisp  uses  a  programmer-visible
features interactive exect, tion wiflt incremental compilation, and  iudircction  mechanism  to  reference  pr~x:edure  activations,
| implementation  | portability.  | These  | features  | of  | modern  |           |                     |             |     |                    |     |
| --------------- | ------------- | ------ | --------- | --- | ------- | --------- | ------------------- | ----------- | --- | ------------------ | --- |
|                 |               |        |           |     |         | whe~'cas  | Ihe  Sinalltalk-80  | programmer  |     | treats  procedure  |     |
programming systems are among the most difficult tu implement
|                    |                |         |                         |     |     | actiwttioas just  | like  any  | other  | data  objects.  |     |     |
| ------------------ | -------------- | ------- | ----------------------- | --- | --- | ----------------- | ---------- | ------ | --------------- | --- | --- |
| efficiently, even  | individually.  | A  new  | implemelltation of the  |     |     |                   |            |        |                 |     |     |
Small/alk-80  system,  hnsted  on  a  sinall  microprocessor-based  The  Sinalltalk-80  language  approaches  programming  with
computer, achieves high performance while retaining' complete
generic data types through message-passing and dynamic typing.
(object code) compatibility with existing implementations.  This  To invoke a pl'Occdure (method in Smalltalk-80 terminology), a
paper  discusses  the  most  significant  optimization  techniques  message is sent to a data object (the receiver), which selects the
developod over the course of the project, many of which are  method to be c×ecuted.  'Ibis means that a method address must
applicable  to  other  languages.  The  key  idea  is  to  represent  bc found at runtime.  At a given lexical point in the code, only
certain  nmtime state (both code and data)  in more than  one  die message name (selector) is known.  To perform a message-
form.  and  to  convert  between  fo~xns when  needed.  send, the data type (class) of file receiver is extracted, and the
|     |     |     |     |     |     | selector is  | used  as a  hash  | index  | into  a  table of the message  |     |     |
| --- | --- | --- | --- | --- | --- | ------------ | ----------------- | ------ | ------------------------------ | --- | --- |
*Smalhalk-80i s a trademark of the XeroxC orporalion.
|     |     |     |     |     |     | dicliottary of the class, which maps selectors to methods.  |     |     |     |     | The  |
| --- | --- | --- | --- | --- | --- | ----------------------------------------------------------- | --- | --- | --- | --- | ---- |
task of melhod-lookup is complicated by the inherilance property
BACKGROUNI)  of classes -- a cla~  may be defined as a  subclass to another,
|     |     |     |     |     |     | inheriting all of the methods of the supcrclass.  |     |     |     | If the initial  |     |
| --- | --- | --- | --- | --- | --- | ------------------------------------------------- | --- | --- | --- | --------------- | --- |
The Smalltalk-80 system is an object-oriented programming
language  and  interactive  programming  environment.  The  method-lookup fails, the lookup algorithm tries again usirlg the
Smalltalk-80  language  inclodes many  of the  most  difficult-to-  message  dictionary  of  the  superclass  of  the  receiver's class,
implement features of modern progralnming languages: dynamic  continuing  in  this way up  the class hierarchy until  a  method
|     |     |     |     |     |     | cnrresponding  | to  the  | selector  | is  found  or  | the  top  | of  the  |
| --- | --- | --- | --- | --- | --- | -------------- | -------- | --------- | -------------- | --------- | -------- |
storage allocation, full upward funargs, and call-time binding of
procedure names to actual procedures based on dynamic type  inheritance  hierarchy  is  reached.
| information, sometimes called message-pa~#tg.  |     |     |     | The interactive  |     |     |     |     |     |     |     |
| ---------------------------------------------- | --- | --- | --- | ---------------- | --- | --- | --- | --- | --- | --- | --- |
The Smalltalk-80 language uses the organization of objects
environment includes a full complement of programming tools:
|     |     |     |     |     |     | into classes to  | provide strong information hiding.  |     |     | Only  | the  |
| --- | --- | --- | --- | --- | --- | ---------------- | ----------------------------------- | --- | --- | ----- | ---- |
compiler, debugger, editor, window system, and so on, all written
in the Smalltalk-80 language itself.  A detailed overview of the  methods associated with a  given class (and  its subclasses) can
system  appears  in  SCG  8l.  Goldberg  83  is  a  technical  access directly the state nf an instance of that class.  All access
|            |                                   |     |             |     |           | from "outside" must be through messages.  |     |     |     | Ik'cause of this, a  |     |
| ---------- | --------------------------------- | --- | ----------- | --- | --------- | ----------------------------------------- | --- | --- | --- | -------------------- | --- |
| refcrcncc  | for  both  file  nnn-interactive  |     | programmer  |     | and  the  |                                           |     |     |     |                      |     |
Smalltalk-80 program must often make procedure calls to access
| system implcmentor; Goldberg  |          | 841 is a reference manual for the  |     |     |     |                                 |     |                                    |        |          |            |
| ----------------------------- | -------- | ---------------------------------- | --- | --- | --- | ------------------------------- | --- | ---------------------------------- | ------ | -------- | ---------- |
|                               |          |                                    |     |     |     | state  where I,mguages such     |     | as Pascal                          | could  | compile  | a  direct  |
| interactive                   | system.  |                                    |     |     |     |                                 |     |                                    |        |          |            |
|                               |          |                                    |     |     |     | access to a tield of a record.  |     | This makes the performance of the  |        |          |            |
SPE('IAL l)l I,'FICULTil,;S  method-lookup  algorithm  even  more  critical.
| The standard   | Smalltalk-80 system implementation is based  |                 |     |      |           | IMPLEMENTATION OUTLINE  |     |     |     |     |     |
| -------------- | -------------------------------------------- | --------------- | --- | ---- | --------- | ----------------------- | --- | --- | --- | --- | --- |
| on  an  ideal  | virtual machine                              | or  v-machine.  |     | The  | compiler  |                         |     |     |     |     |     |
The purpose of the research de~ribed here was to build a
| generates  | code  for  this  | machine,  | and  the  | implementor's  |     |     |     |     |     |     |     |
| ---------- | ---------------- | --------- | --------- | -------------- | --- | --- | --- | --- | --- | --- | --- |
Smalltalk-80 system with acceptable performance on a relatively
documentation describes the system as an interpreter for the v-
|     |     |     |     |     |     | inexpensive,  | microproecssor-based  |     | computer;  | specifically,  | to  |
| --- | --- | --- | --- | --- | --- | ------------- | --------------------- | --- | ---------- | -------------- | --- |
discover how to implement the basic data and code objects of
the Smalltalk-80 system in a way that still conformed to the v-
Permission to copy without fee all or part of this material is granted
machine specification, but were more suitable for conventional
provided that the copies are not made or distributed for direct
commercial advantage, the ACM copyright notice and the. title of the  hardware.  (As of early 1982, the only implementations that ran
|     |     |     |     |     |     | at  acceptable  | speed  | were  | on  non-commercial,  |     | user-  |
| --- | --- | --- | --- | --- | --- | --------------- | ------ | ----- | -------------------- | --- | ------ |
publication and its date appear, and notice is given that copying is by
|     |     |     |     |     |     | microprogra,nmable  | roachines,  | as  | de~ribed  | in  Krasner  | 83  |
| --- | --- | --- | --- | --- | --- | ------------------- | ----------- | --- | --------- | ------------ | --- |
permission of the Association for Computing Machinery. To copy
|     |     |     |     |     |     | I.ampson  | 81.)  The  | system  specification  |     | in  Goldberg  | 83  |
| --- | --- | --- | --- | --- | --- | --------- | ---------- | ---------------------- | --- | ------------- | --- |
otherwise, or to republish, requires a fee and/or specific permission.
includes tile definition of internal data structttres and object code
|     |     |     |     |     |     | representation                             | for the virtual  | machine.  | Indeed,  | much              | of the  |
| --- | --- | --- | --- | --- | --- | ------------------------------------------ | ---------------- | --------- | -------- | ----------------- | ------- |
|     |     |     |     |     |     | system code depends on these definitions.  |                  |           |          | We chose to take  |         |
©  1983 ACM 0-89791-125-3/84/001/0297  $00.75  these definitions as given,  rather than  alter  the  system code.
297

"lhis was motivated partly by a desire to retain object-code and Smalltalk-80 v-machines, use a stack-oriented
portability, and pardy by a desire not to complicate the architecture for convenience in code generation, but
description of the SmaUtalk-80 machine model. most available hardware machines execttte register-
oriented code much more efficiently than stack-oriented
The single principle that underlies all the results reported code.
here is dynamic change of representation. By this we mean that
the same infi)rmation is represented in more than one * The basic operations of the v-machine may be
(structurally different) w,~y during its lifetime, being converted relatively expensive to implement, even though the
transparently between representations a:; needed for efficient use overall algorithm represeqted by a v-code program may
at any moment. An important special case of this idea is not be much more expensive than if it were
caching: one can think of information in a cache as a different implemented in the hardware instruction set. For
representation of the same information (considering contents and example, even though a naive interpreter for the
accessing information together) in the backup memory. In the Smalltalk-80 v-code must perform rcl~:renee counting
implementation described in this paper, we applied this principle operations every time it pushes a variable value onto the
to several different kinds of runtime information in the stack, a sequence of several instructions often has no net
Smalltalk-80 system. effect on reference counts.
* We dynamically translate v-code (i.e., code in the If the v-code were translated to n-code after normal
instruction set of the v-machine) into code that executes compilation of a source program to v-code, the interpreter's
directly on the hardware without interpretation, the overhead could be eliminated and some optimizations become
native code or n-code. Translated code is cached: it is possible. One technique for eliminating part of the overhead of
regenerated rather than paged. interpretation is threaded code Bell 73 Moore 741. In this
approach, v-code consists of an actual sequence of subroutine
* We represent procedure activation records (contexts in calls on runtime routines. This technique does reduce the
Smalltalk-80 parlance) in either a machine-oriented form, o~;erhead for fetching and dispatching v-code instructions,
when they are being used to hold execution state, or in although it does not help with operaod decoding, or enable
the form of Smalltalk-80 data objects, when they are optimizations that span more than one v-instruction. We prefer
being treated as such. to translate v-code to in-line n-code in a more sophisticated way.
Naive translation from v-code to n-code is a process
* We use several different caches to speed up the
something like macro-expansion. In fact, Mitchell 71 observed
polymorphic search required at each procedure
that a translator can be derived very simply from an interpreter
invocation. In the best case, which applies over 90% of
by having the interpreter save its action-routine code in a buffer
the time, a Smalltalk-80 procedure invocation requires
rather than executing it. If the computation performed by
only one comparison operation in addition to a
individual action routines is small relative to the computation
conventional procedure linkage.
needed for the interpreter loop, the benefit of even this simple
* Using the techniques in Deutsch&Bobrow 76, we kind of translation will be great.
represent reference count information for automatic
Translation-time can also be considered an opportunity for
storage management in a way that eliminates
peephole optimization or even mapping stack references to
approximately 85% of the reference counting operations
registers Pittman 80. Translation back-ends for portable
required by a standard implementation.
compilers have been implemented Zellweger 79.
CODE TRANSLATION
DYNAMIC TRANSI,ATION
Targeting code to a portable v-machine has been used in
Because the Smalltalk-80 v-code is a compact representation
other language implementations. Usually v-code targeting is
that captures the basic semantics of the language, n-code will
used only to avoid having multiple (one per target machine)
typically take up much more space than v-code. (In the
code-generation phases of the compiler; a secondary benefit is
implementation discussed in this paper, n-code takes about 5
that v-code is usually much more compact than code for any real
times as much space as v-code.) This would place severe stress
machine. Since the Smalltalk-80 compiler is just one tool
on a virtual memory system if the n-code were being paged.
available in the same interactive environment used for execution,
However, since n-code is derived algorithmicafiy from v-code,
and other tools besides the compiler must be able to examine the
there is no need to keep it permanently: it can be recomputed
machine state, the v-machine approach is even more attractive in
when needed, if this is more efficient than swapping it in from
reducing the cost of rehosting.
secondary storage. This leads us to the idea of translating at
runtime. (The idea of dynamic translation appears in Rau 78,
PERFORMANCEI SSUI ~ where it is applied to translation from v-code to microcode.)
When a procedure is about to be executed, it must exist in n-
To rehost the system, an implementor must emulate the v-
code form. If it does not, the call faults and the translator takes
machine on the target hardware, either in microcode or in co,ltrol. The translator finds the corresponding v-code routine,
software. This normally incurs a severe performance penalty translates it, and completes the call. Since, as mentioned earlier,
arising from several factors. the translation process is more akin to macro-expansion than
compilation, translation time for a v-code byte is comparable to
* Processors have specialized hardware for fetching,
the time taken to interpret it.
decoding, and dispatching their own native instruction
set. This hardware is typically not available to the We consider the translation approach, and dynamic
prngrammcr (although it may be available at the translation in partietdar, to be the most interesting part of our
microprogram level), and therefore not useful to the v- research, since it motivated the work on multiple state
machine interpreter in its time-consuming operation of rcprcsentations described below. A later section of this paper
instruction fetching, decoding, and dispatching. presents the experimental results that support our contention that
dynamic translation is an effective technique in a substantial
* The v-machine architecture may be substantially
region of current technological parameters.
different from that of the underlying hardware. :or
example, many v-machines, including both the P-system
298

referenced as a data object, and can be freed as soon as control
MAPPING STNI'E AT RUNTIME  returns from  them.  (Note  that any context in  which a  block
|                 |                  |                |               |              |                   | context is created does not satisfy this criterio,1.)  |     |     |     |     | Such contexts    |     |
| --------------- | ---------------- | -------------- | ------------- | ------------ | ----------------- | ------------------------------------------------------ | --- | --- | --- | --- | ---------------- | --- |
| Since           | the  definition  | of the         | Smalltalk-80  |              | v-machine makes   |                                                        |     |     |     |     |                  |     |
|                 |                  |                |               |              |                   | are candidates for stack-frame representation.         |     |     |     |     | (An unpublished  |     |
| runtime  state  | sucl~            | as  procedure  |               | activations  | visible  to  the  |                                                        |     |     |     |     |                  |     |
progrannner as data objects, an implementation based on n-code  experimental implementation of an earlier Smalltalk system used
|     |     |     |     |     |     | linear  stacks,  | but  | did  not  | deal  properly  |     | with  contexts  | that  |
| --- | --- | --- | --- | --- | --- | ---------------- | ---- | --------- | --------------- | --- | --------------- | ----- |
must find a way to make the state appear to the programmer as
|     |     |     |     |     |     | outlived  | their  callers.)  |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --------- | ----------------- | --- | --- | --- | --- | --- |
though it were the state of a v-machine, regardless of the actual
representation.  The  system must maintain  a  mapping of n-  Stack  allt~cation  of contexts solves one  of the  two  major
machine st.'tte to v-machine state; in particular, it nmst keep the  efficiency problems associated with  treating contexts like other
| v-code  a~ailable  | for  | inspection.  |     |     |     |              |                                                     |                |     |             |      |           |
| ------------------ | ---- | ------------ | --- | --- | --- | ------------ | --------------------------------------------------- | -------------- | --- | ----------- | ---- | --------- |
|                    |      |              |     |     |     | objects,     | namely                                              | the  ovcrbead  | of  | allocating  | the  | contexts  |
|                    |      |              |     |     |     | themselves.  | [l)ctltmh&llobrow 76] shows how to solve the other  |                |     |             |      |           |
How can we guarantee that all attempts to access a quantity
requiring representation mapping are detected?  The structure of  problem,  of  reference  counting  operations  apparently  being
|     |     |     |     |     |     | required  | on every store into a local variable.  |     |     |     | With these two  |     |
| --- | --- | --- | --- | --- | --- | --------- | -------------------------------------- | --- | --- | --- | --------------- | --- |
the Smalltalk-80 language guarantees that the only code that can
|                                          |             |           |        |                             |                      | problems      | solved,  | we  can  rise  | the        | hardware  | subroutine  | call,  |
| ---------------------------------------- | ----------- | --------- | ------ | --------------------------- | -------------------- | ------------- | -------- | -------------- | ---------- | --------- | ----------- | ------ |
| access  an                               | object  of  | a  given  | class  | directly                    | is  the  code  that  |               |          |                |            |           |             |        |
|                                          |             |           |        |                             |                      | return,  and  | store    | instructions   | directly.  |           |             |        |
| implements messages sent to that class.  |             |           |        | 'lllus, the only code that  |                      |               |          |                |            |           |             |        |
can directly access the parts of an object requiring mapping is  Ottr system has several types of context representations.  A
| code associated with that object's class.  |     |     |     | Recall that all the code  |     |     |     |     |     |     |     |     |
| ------------------------------------------ | --- | --- | --- | ------------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
message-send creates a new context in a representation optimized
| in  the  Smalltalk-80  |     | system  | is  written  | in  | the  Snlalitalk-80  |     |     |     |     |     |     |     |
| ---------------------- | --- | ------- | ------------ | --- | ------------------- | --- | --- | --- | --- | --- | --- | --- |
for execution: a frame is allocated on the machine's stack (with
| language, hence compiled  |     | into  | v-code.  | When  | we  translate a  |                       |     |                     |     |                |     |          |
| ------------------------- | --- | ----- | -------- | ----- | ---------------- | --------------------- | --- | ------------------- | --- | -------------- | --- | -------- |
|                           |     |       |          |       |                  | some spare slots) by  |     | the  usual machine  |     | instructions.  |     | In  the  |
p~x~cedure from v-code to n-code that is asst~.'iatcd  with a class  simple case, where no reference is ever made to the context as a
whose representation may require mapping, we generate special  data  object,  the  machine's return  iristruction  simply pops  the
n-code  that  calls  a  subroutine  to  ensure  that  the  object  is  fi'ame off file stack when control returns fi'om the context.  This
| represented  | in  a  form  | where  | accesses to  | its named  | parts are  |                                             |     |     |     |                       |     |     |
| ------------ | ------------ | ------ | ------------ | ---------- | ---------- | ------------------------------------------- | --- | --- | --- | --------------------- | --- | --- |
|              |              |        |              |            |            | kind of context, which lives its life as a  |     |     |     | stack frame, we call  |     |     |
| meaningful.  |              |        |              |            |            | volatile.                                   |     |     |     |                       |     |     |
The most obvious quantity requiring mapping is the return
|     |     |     |     |     |     | At  the  | other  | extreme,  | we  store  | contexts  | in  | a  format  |
| --- | --- | --- | --- | --- | --- | -------- | ------ | --------- | ---------- | --------- | --- | ---------- |
address (PC) in an activation record, whicll refers to a location in
compliant with the virtual machine specification, which can be
the n-code procedure rather than in the v-code.  Although there  manipttlated as data  items.  We call  this representation stable.
is no simple algorithtnic correspondence between the v-PC and
The third representation of a context, called hybrid, is a stack
| the  n-PC  | values,  the  | v-PC  | need  | only  be  | available  when  a  |     |     |     |     |     |     |     |
| ---------- | ------------- | ----- | ----- | --------- | ------------------- | --- | --- | --- | --- | --- | --- | --- |
program attempts to inspect an activation as a data objcct.  At  frame that incorporates header information to make it look partly
that  moment,  the  system  can  consult  (or  compute)  a  table  like an ordinary data object.  A volatile context is converted to
associated  with  the  procedure  that  gives  the  correspondence  hybrid when a  pointer is generated  to it.  Since this makes it
between  n-  and  v-PC  rallieS.  possible fi~r programs to refer to the context as an object, we fill
|     |     |     |     |     |     | in slots in the frame corresponding to the header  |     |     |     |     | fields in an  |     |
| --- | --- | --- | --- | --- | --- | -------------------------------------------------- | --- | --- | --- | --- | ------------- | --- |
We can greatly reduce the size of the mapping tables for PC  ordinary object.  This pseudo-object is tagged as being of a class
values by observing that the PC can only be accessed when an  we name "l)ummyContext."  A block of memory is allocated,
activation  is  suspended,  i.e.,  at  a  procedure  call  or  and its address is stored in the context in case the context must
| interrupt/process-switch.  |     | If we are willing to accept somewhat  |     |     |     |                               |     |                                      |     |     |     |     |
| -------------------------- | --- | ------------------------------------- | --- | --- | --- | ----------------------------- | --- | ------------------------------------ | --- | --- | --- | --- |
|                            |     |                                       |     |     |     | be stabilized in the future.  |     | Since there may be pointers to this  |     |     |     |     |
greater  latency  in  a  Smalltalk-80  program's  response  to  context,  it cannot be  returned  fiom in a  normal  way, so the
interrupts,  we  can  choose  a  restricted  but  sufficient  set  of  return  address  is  copied  to  another  slot  in  the  frame  and
allowable interrupt points, and only store the mapping tables for  replaced with the address of a clean-up routine that stabilizes the
| those points.  | This is what our implementation does: interrupts  |     |     |     |     |              |          |     |     |     |     |     |
| -------------- | ------------------------------------------------- | --- | --- | --- | --- | ------------ | -------- | --- | --- | --- | --- | --- |
|                |                                                   |     |     |     |     | context  on  | return.  |     |     |     |     |     |
are only allowed at, and PC map entries are only stored for, all
prtx:cdure calls and backward branches (the latter since interrupts  When a message is sent to a hybrid context, the send fails
must  be  allowed  inside  loops).  (there are no procedures defined for the DummyContext class),
|     |     |     |     |     |     | and  a  routine  | is called  | to convert  |     | the  hybrid context  |     | to  the  |
| --- | --- | --- | --- | --- | --- | ---------------- | ---------- | ----------- | --- | -------------------- | --- | -------- |
MUI,TIPLE REI)RI'2"iENTA'I'IONSO F CONTEXTS  stabilized form.  At this point PC mappitlg comes into play; the
|               |           |                    |                      |                           |             | n-PC in the activation is converted to a  |                                                  |     |     | v-PC for the stabilized  |     |     |
| ------------- | --------- | ------------------ | -------------------- | ------------------------- | ----------- | ----------------------------------------- | ------------------------------------------------ | --- | --- | ------------------------ | --- | --- |
| As mentioned  | earlier,  | the                | format of procedure  |                           | activation  |                                           |                                                  |     |     |                          |     |     |
|               |           |                    |                      |                           |             | representation.                           | Poi,lters to the hybrid context are switched to  |     |     |                          |     |     |
| records  are  | part  of  | the  Smalltalk-80  |                      | v-machine specification.  |             |                                           |                                                  |     |     |                          |     |     |
refer to the stable context (this is simple in our system, which
| Contexts  | are  full-fledged  | data  | objects;  | they  | have  identifiable  |                                              |     |     |     |                        |     |     |
| --------- | ------------------ | ----- | --------- | ----- | ------------------- | -------------------------------------------- | --- | --- | --- | ---------------------- | --- | --- |
|           |                    |       |           |       |                     | uses an indirection table for all objects).  |     |     |     | After the context has  |     |     |
fields which can be accessed and they respond'to messages.  A  been stabilized, tile failed mess,age is re-sent to the stable form.
| context is created for every message-send.  |     |     |     | There is also syntax  |     |           |             |               |     |                  |     |            |
| ------------------------------------------- | --- | --- | --- | --------------------- | --- | --------- | ----------- | ------------- | --- | ---------------- | --- | ---------- |
|                                             |     |     |     |                       |     | A stable  | context is  | not suitable  |     | for  execution.  |     | Before  a  |
in the language for creating contexts whose activation is deferred,
stabilized context can be resumed, it is reconstituted on the stack
| cldled  block  | contexts  | in  | Smalltalk-80  | terminology,  | which  |     |     |     |     |     |     |     |
| -------------- | --------- | --- | ------------- | ------------- | ------ | --- | --- | --- | --- | --- | --- | --- |
the functional&  closures,  or funargs  as  hybrid.  Again,  this  means  that  the  n-PC  must  be
| correspond  | to  |     |     |     | of  other  |     |     |     |     |     |     |     |
| ----------- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
languages.  Most control structures in the Smalltalk-80 system are  reconstructed fi'om tile v-PC.  Usually the v-PC does not change
during the stable period, so our system includes a one-element
| implemented  | with  | block  contexts.  |     |     |     |     |     |     |     |     |     |     |
| ------------ | ----- | ----------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
cache ill each n-code procedure for tile most recent v-PC/n-PC
The fact that contexts are standard data objects implies that  pair,  to  avoid  having  to  run  the  mapping  algorithm.
they must be created like data objects, i.e., allocated on a heap
|                              |     |          |               |                |              | Block contexts are "~boro"  |               |         | in stable  | form, since the whole  |     |          |
| ---------------------------- | --- | -------- | ------------- | -------------- | ------------ | --------------------------- | ------------- | ------- | ---------- | ---------------------- | --- | -------- |
| and  reclaimed               | by  | garbage  | collection    | or  reference  | counting.    |                             |               |         |            |                        |     |          |
|                              |     |          |               |                |              | purpose                     | of  closures  | is  to  | provide    | a  representation      |     | for  an  |
| Unforttmately. conventional  |     |          | machines are  | adapted        | for calling  |                             |               |         |            |                        |     |          |
sequences that create a new activation record as a stack flame,  execution  context  which  can  be  invoked  later.
| storing  suspended  |     | state  in  | predefined  | slots  | in  the  frame.  |     |     |     |     |     |     |     |
| ------------------- | --- | ---------- | ----------- | ------ | ---------------- | --- | --- | --- | --- | --- | --- | --- |
IN-I,INE CACI lING OF METHOI) Ai)I)R I~JSES
| Actually  | implementing contexts  |     | as  | heap  objects  | results  in  a  |     |     |     |     |     |     |     |
| --------- | ---------------------- | --- | --- | -------------- | --------------- | --- | --- | --- | --- | --- | --- | --- |
serious performance  penalty.  Mess~tge-passing is applied down to the simplest operations
|                     |     |       |       |                   |            | in  Smalltalk.  | The  | system provides  |     | a  variety  | of  predefined  |     |
| ------------------- | --- | ----- | ----- | ----------------- | ---------- | --------------- | ---- | ---------------- | --- | ----------- | --------------- | --- |
| Mcasttrements show  |     | that  | even  | in  Smalltalk-80  | programs,  |                 |      |                  |     |             |                 |     |
classes: the most basic operations on.elementary data types (such
more than 85% of all contexts behave like procedure activations  as addition of integers) are performed by primitives implemented
| in  conventional  | languages:  |     | they are  | created  | by  a  call,  never  |     |     |     |     |     |     |     |
| ----------------- | ----------- | --- | --------- | -------- | -------------------- | --- | --- | --- | --- | --- | --- | --- |
299

by the kernel of die system, rather Ih;in by Smalltalk routines,  the one-clement in-line cache and linked sends for accelerating
but there is no distinction drawn at the language level.  Since  method-lookup,  and  the  technique  of  v-codc  to  n-code
mes.~ge-sends are so ubiquitous, they must bc fast: the operation  translation  (specifically,  dynamic  translation).
| 6f  method-lookup  | is  | both  expensive and  |     | critical.  |     |     |     |     |     |     |     |     |
| ------------------ | --- | -------------------- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- |
CON'I'I£XTR   I':PRF~SENTATIONS
All existing Smalltalk-80 implementations accelerate method-
lookup by using a method cache, a hash table of pooular method  The dramatic drop in reference counting overhead obtained
addresses indexed by the pair (receiver class, message selector).  by  treating contexts specially has been  documented elsewhere
|     |     |     |     |     |     | (e.g.,  | Krasqcr  | 83,  section  | 19).  | We  also  | obtain  a  | striking  |
| --- | --- | --- | --- | --- | --- | ------- | -------- | ------------- | ----- | --------- | ---------- | --------- |
This simple technique typically improves system perfi~rmance by
20-30%.  More  extensive  measurements  of this  improvement  efficiency improvement by allocating contexts oil a stack, and by
appear  in  Krasner  83.  keeping their contents  in  execution-oriented form.  Off`setting
|          |              |                   |     |            |          | these     | advantages,    | in  our   | implementation  | there            | is  an  | added  |
| -------- | ------------ | ----------------- | --- | ---------- | -------- | --------- | -------------- | --------- | --------------- | ---------------- | ------- | ------ |
| Further  | performance  | improvements are  |     | suggested  | by  the  |           | of converting  |           |                 |                  |         |        |
|          |              |                   |     |            |          | overhead  |                | coqtcxts  | between         | volatile/hybrid  |         | and    |
observation of dynamic locality of lype usage.  That is, at a given  stable fi}rms, and of ensuring that a context accessed as a data
point in code, the receiver is often the same class as the receiver  object (either by sending it a message or directly while running a
at the same point when the code was last exect,ted.  If wc cache  method  ilnplcmentcd  in  a  context class)  is  in  stable  form.
the looked-up method address at the point of send, subsequent  3'o evaluate  the  perfi~rmance advantage  of linear  context.
execution of the send code has the method address at hand, and
allocation and volatile rcpresentatinn, we compared our code for
method-lookup can be avoided if the class of the receiver is the  allocating and  deallocating  contexts against  code  based  on  a
same as it was at the previous execution of this particular send.  hypothetical design that used the standard object representation
Of course, the class of the receiver may have changed, and must  for contexts, but did  not reference-count their contcnts.  This
be checked against the class corresponding to the cached method
|     |     |     |     |     |     | code appears to  |     | take about 8 times as hmg to exccutc, which  |     |     |     |     |
| --- | --- | --- | --- | --- | --- | ---------------- | --- | -------------------------------------------- | --- | --- | --- | --- |
address.  would nlakc it consume 12°o of total execution time compared to
|          |                 |            |        |      |             | 1.5% for  | our  | present  code.  |     |     |     |     |
| -------- | --------------- | ---------- | ------ | ---- | ----------- | --------- | ---- | --------------- | --- | --- | --- | --- |
| In  the  | implementation  | described  | here,  | the  | translator  |           |      |                 |     |     |     |     |
generates n-code for sends unlit~ked -- as a call to the method-
I,ess than 10CO of all co,~texts cvcr exist in othcr than volatile
lookup routine, with the selector as an in-line argument.  The  fibrin,  l~lock contexts, which arc created in stable fi~rm, and their
method-lookup  routine  links  the  call  by  finding the  receiver  cnclosing context,  which  must  be  madc  hybrid  so  the  block
class, storing it in-line at the call point, and doing the method-  context can refer to it, account for two-thirds of these: nearly all
| lookup  (like  | other  | implementations,  | it  uses  | a  selector/class-  |     |     |     |     |     |     |     |     |
| -------------- | ------ | ----------------- | --------- | ------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
of the remainder arise fi'om an implcmcntation detail rcgarding
method cache).  When the n-code method address is found, it is  linkiqg  togcther  fixed-size  stack  segments.  n  all  of  our
placed in-line with a call instruction, overwriting the former call  measured  examples,  the  time  rcquired  for  thc  conversion
to the lookup routine.  "'he  call is then re-executed.  (Of course,  between the stable and  volatile  form  was  under  3CO of total
| there may be no corresponding n-code method, in which case the  |     |                                      |     |     |     | execution  | time.  |     |     |     |     |     |
| --------------------------------------------------------------- | --- | ------------------------------------ | --- | --- | --- | ---------- | ------ | --- | --- | --- | --- | --- |
| translator is called firsL)                                     |     | Note that this is a kind of dynamic  |     |     |     |            |        |     |     |     |     |     |
code  modification,  which  is generally condemned  in  modern  If the receiver of a message is not a hybrid context, there is
practice.  The n-method address can just as well be placed out-  no overhead for making the check bccausc it happens as part of
of-line  and  accessed  indirectly;  c~de  modificatioll  is  more  the normal mcthnd-k)okup (recall that hybrid contexts appear to
be objects of a special class DummyContcxt with no associated
| •  cl~cicnt,  | and  we  are  | using  it  | in  a  weIFconfined  |     | way.  |            |       |                      |     |             |        |       |
| ------------- | ------------- | ---------- | -------------------- | --- | ----- | ---------- | ----- | -------------------- | --- | ----------- | ------ | ----- |
|               |               |            |                      |     |       | methods).  | Only  | when  method-loukup  |     | fails is a  | check  | made  |
The  entry  code  of an  n-code  method  checks  the  stored  whether the  receiver was actually  a  DummyCoqtext.  In  the
recei~crclass from the point of call against the actoal receiver  normal  operation  of  the  system,  mcssagcs  are  only  sent  to
class.  If they do not match, relinking must ¢~:cur, just as if the
contexts by thc debugger and for cleanup during dcstruction of a
call  had  not  yet  been  linked.  process, so  the  overall  impact  is  negligible.
Since linked sends have n'code method addresses bound in-
As di~usscd above, methods associated with context classes
line, this address must be invalidated if the called n-code method  must be translated specially, so that each rcfcrence to an instance
is being discarded from memory.  The idea of" scanning all n-  variablc chccks to makc snrc the rcccivcr is in stable form.  The
code  routines to  invalidated  linked  addresses was  initially  so  time  required  for  this  check  is  negligible.
| daunting that we almost rejected the scheme.  |     |     |     | However, since n-  |     |     |     |     |     |     |     |     |
| --------------------------------------------- | --- | --- | --- | ------------------ | --- | --- | --- | --- | --- | --- | --- | --- |
code only exists in main memory, invalidation cannot produce  IN-LINE CACIIE AND ,INKED SENDS
| time-consuming page faults.  |     | Furthermore. since the PC mapping  |     |     |     |     |     |     |     |     |     |     |
| ---------------------------- | --- | ---------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
Independent measurements by us and by a group at U.C.
tables described earlier contain precisely the addresses of calls in
Bcrkcley confirm that the one-element in-line cache is cffective
| the n-code, no searching of the n-code is required:  |     |     |     |     | it is only  |     |     |     |     |     |     |     |
| ---------------------------------------------------- | --- | --- | --- | --- | ----------- | --- | --- | --- | --- | --- | --- | --- |
necessary to go through the mapping tables and overwrite the  about 95% of the time.  Measuremcnts reported in Krasner  83
call instructions to which the entries point.  (A scheme similar to  indicate that a more conventional global cache of a reasonable
|            |            |           |       |     |     | size is effective about 85-90% of the time.  |     |     |     | It may be that an in-  |     |     |
| ---------- | ---------- | --------- | ----- | --- | --- | -------------------------------------------- | --- | --- | --- | ---------------------- | --- | --- |
| this  may  | be  found  | in  Moon  | 73.)  |     |     |                                              |     |     |     |                        |     |     |
linc cache tends to lower the effectiveness of the global cache,
For a few special selectors like  +,  the translator generates  since most of thc Iookups that would socceed in the global cache
in-line code fi~r the common case along with the standard send  are now handled by the in-line cache, but we have no direct
code. For example. -I- generates a class check to verity that both  evidence on  this  point.
argnments are small integers, native code for integer addition,
and an overflow check on the result.  If any of the checks fail,  Adding an  in-line cache to the simple translator described
the send code is execrated.  This is a space-time tradeoff justified  below  improved  overall  performance  by  only  9%.  On  a
benchmark consisting ahnost entirely of message sends where the
by measurements that indicate that the ovcrwhehning majority of
in-'line cache is guaranteed valid, the in-line cache only improved
| arithmetic operations invoh'c only small  |     |     | integers, even  |     | though  |     |     |     |     |     |     |     |
| ----------------------------------------- | --- | --- | --------------- | --- | ------- | --- | --- | --- | --- | --- | --- | --- |
they are (in principle) polymorphic like all other operations in  pcrforlnanc¢ by 11%. 'l'llc improvement obtained by adding an
the  language.  in-line cache to the optimizing translator was also about  L0%.
|     |     |     |     |     |     | Our  | original  | hand-analysis  | indicated  | that  | the  | overall  |
| --- | --- | --- | --- | --- | --- | ---- | --------- | -------------- | ---------- | ----- | ---- | -------- |
improvement should be closer to 20%, and we cannot yet account
| EXPERIMENTAL  | R F,S ULTS  |     |     |     |     |           |               |      |                 |     |                  |     |
| ------------- | ----------- | --- | --- | --- | --- | --------- | ------------- | ---- | --------------- | --- | ---------------- | --- |
|               |             |     |     |     |     | for  the  | discrepancy.  | The  | code  produced  | by  | the  optimizing  |     |
Three aspects of our results deserve experimental validation:
the use of stable and volatile context representations, the use of
300

translator for the activate-and-return benchmark is a remarkable
|     |     |     |     |     |     | *   | Pure interpretation:  | only  v-code exists; it is brought  |     |     |
| --- | --- | --- | --- | --- | --- | --- | --------------------- | ----------------------------------- | --- | --- |
47% faster than the code from the simple translator with the in-  into  main  memory  as  needed.
line cache, s'lggesting that operations other than the overhead
eliminated by the in-line cacl~e still dominates overall execution  *  Static translation:  n-code is generated simultaneously
time.  with v-code.  Only n-code is needed at execution time.
|     |     |     |     |     |     | N-code  | is  brought  | into  memory  | as  | needed,  |
| --- | --- | --- | --- | --- | --- | ------- | ------------ | ------------- | --- | -------- |
I)YNAMICC  OIIE "I'IIANSLATION
* Dynamic translation: n-code is kept in a cache in main
Our  implementation  of  the  Smalltalk-80  v-machine  is  memory: v-code is brought into memory for translation
| designed  to  | be  easily                                            | switchable  | between  | different execution  |     | as  | needed.  |     |     |     |
| ------------- | ----------------------------------------------------- | ----------- | -------- | -------------------- | --- | --- | -------- | --- | --- | --- |
| strategies.   | We have implemented a straightforward interpreter, a  |             |          |                      |     |     |          |     |     |     |
simple  translator  with  almost  no  optimization,  and  a  more  Note that space taken by n-code in main memory trades off
sophisticated translator.  Both translators exist in two  variants,  against space  for data.  When main memory space is needed
with and without the in-line cache described above.  Switching  (either fi)r n-code or for da~0, we have the option of replacing
|                     |                                               |     |     |     |     | data pages or discarding n-code.  |     | Unfortunately, since the work  |     |     |
| ------------------- | --------------------------------------------- | --- | --- | --- | --- | --------------------------------- | --- | ------------------------------ | --- | --- |
| between strategies  | simply requires relinking the implementation  |     |     |     |     |                                   |     |                                |     |     |
with a different set of modules; the price in execution speed paid  described here has been carried out in a  non-virtual memory
for  this  flexibility  is  negligible.  environment, we  have  no  experimental  results on  this topic.
Our  first  experiment  in  code  translation  was  a  simple  CONCLUSIONS AND RELATED WORK
| translator  | that  does  | little  peephole  | optimization  |     | and  always  |     |     |     |     |     |
| ----------- | ----------- | ----------------- | ------------- | --- | ------------ | --- | --- | --- | --- | --- |
Perhaps the most intportant observation from our research is
| generates exactly  | 4 n-bytes per v-byte.  |     |     | Clhe latter restriction  |     |                                                |     |     |     |                  |
| ------------------ | ---------------------- | --- | --- | ------------------------ | --- | ---------------------------------------------- | --- | --- | --- | ---------------- |
|                    |                        |     |     |                          |     | that we have demonstrated that it is possible  |     |     |     | to implement an  |
eliminated the need for the PC mapping tables described earlier.)
|     |     |     |     |     |     | interactive system  | based on a  | demanding high-level language.  |     |     |
| --- | --- | --- | --- | --- | --- | ------------------- | ----------- | ------------------------------- | --- | --- |
Our second experiment was a translator that does significant  with  only  a  modest  increase  in  memory  requirements  and
peephole nptilnization.  The code  it generates keeps  the top  without the use of any of the special hardware (special-purpose
|     |     |     |     |     |     | mierocude,  | tagged  memory architecture, garbage collection co-  |     |     |     |
| --- | --- | --- | --- | --- | --- | ----------- | ---------------------------------------------------- | --- | --- | --- |
element of the v-machine stack in a machine register whenever
possible,  and implements all v-instructions in-line except sends  processor) often advocated for such systems, and with resulting
and  a  few  rare  instructions like  load current context.  Even  perfonnanee that users judge excellent.  We have achieved this
arithmetic and relational operations are implemented in-line, with  by careful optimization of the observed common cases and by
a call on an nttt-of-line routine if the operands arc not small  the plentiful use of caches and other changes of representation.
integers.  The  resulting code  is  bulky  but  fast.  A related  research  project [Patterson 83] is investigating a
Smalllalk-80 implementation that uses only n-code, on a specially
To estimate the space  required by translated methods, we  designed VI,SI processor called SOAR.  As discussed above, this
have observed that the average v-method consists of 55% pointers  implementation requires rewriting the compiler, debugger, and
| (literal constants,  | message  | selectot.'s, and  |     | references  | to  global  |                                                          |     |     |     |     |
| -------------------- | -------- | ----------------- | --- | ----------- | ----------- | -------------------------------------------------------- | --- | --- | --- | --- |
|                      |          |                   |     |             |             | other tools that manipulate compiled code and contexts,  |     |     |     | We  |
variables) and 45% v-instructions.  Since our simple translator  expect some interesting comparisons between the two approaches
expands each v-code byte to 4 n-code bytes, the expansion factor  sometime in  1984, when  the  SOAR  implementation becomes
| for the method as a whole is .55+(.45*4)=2.35.  |     |     |     | The version of  |     | operational.  |     |     |     |     |
| ----------------------------------------------- | --- | --- | --- | --------------- | --- | ------------- | --- | --- | --- | --- |
the simple translator that uses an in-line cache simply triples the
size of the pointer area, leaving room for a cached class and n-  We  believe  the  techniques  described  in  this  paper  are
method pointer regardless of whether the pointer is a selector or  applicable in varying degrees to other late-bound languages such
something else.  This expands the total size of methods by a  as I,isp, and to portable V-code-based  language implementations
| factor of (3*.55)+(4*.45)=3.45.  |     | The observed expansion factors  |     |     |     |                               |     |                                  |     |     |
| -------------------------------- | --- | ------------------------------- | --- | --- | --- | ----------------------------- | --- | -------------------------------- | --- | --- |
|                                  |     |                                 |     |     |     | such as the Pascal P-system,  |     | but we have no current plans to  |     |     |
for  the  optimizing  translators appear  in  the  table  below.  investigate these  other  languages.
| We  ran        | the  standard             | set  | of  Smalltalk-80  |         | benchmarks  | ACKNOWLEDGMENTS  |     |     |     |     |
| -------------- | ------------------------- | ---- | ----------------- | ------- | ----------- | ---------------- | --- | --- | --- | --- |
| described  in  | [Krasner 83], section 9,  |      | using each        | of our  | five        |                  |     |     |     |     |
execution strategies.  The normalized results are summarized in  Thanks are doe to Mike  Braca. who programmed the I/O
the  following  table:  kernel of our implementation: Bob Hagmann, who programmed
the optimizing code translator and made many contributions to
Strategy  Space  Ti.me  the design of the system: and Mark Roberts, who implemented
|     |     |     |     |     |     | the  disk  | file  system  | and  virtual  memory  | capabilities.  | Bob  |
| --- | --- | --- | --- | --- | --- | ---------- | ------------- | --------------------- | -------------- | ---- |
Interpreter  1.00  1.000  Hagmann,  Dan  Ingalls,  and  Paul  McCullough  contributed
|     |     |     |     |     |     | helpfitl comments on this paper.  |     | The Smalludk-80  |     | system itself  |
| --- | --- | --- | --- | --- | --- | --------------------------------- | --- | ---------------- | --- | -------------- |
Simple translator,  2.35  0.686  is  owed  to  PARC  SCG.  Butler  I,ampson  gave  helpful
no in-line cache  suggestions  during  the  early  project  design  phase.
| Simple translator  |     | 3.45  |     | 0.625  |     | R E FER ENCES  |     |     |     |     |
| ------------------ | --- | ----- | --- | ------ | --- | -------------- | --- | --- | --- | --- |
with in-line cache
[Ammann 75] Ammann, U., Nod. Jcnsen, K.. Nageli, H.. "The
Optimizing translator,  5.0  0.564  Pascal (P) Colnpilcr Implementation Notes." Institut Fur
no in-line cache  Inlbrmatik, Eidgenossische Tcchni~he IIochschule, Zurich, 1975.
Optimizing translator  5,03  0.515  [Ammann 77] Ammann, U., "On code generation in a Pascal
with in-line cache  compiler." Software Practice and Experience v7 #3. June/July
1977, pp. 391-423.
The space figure fi)r the optimizing translator without the in-
line cache could be reduced at the expense of further sh)wing the  [Bell 73] Bell. J. R., "Threaded Code." Communications ofthe
| code  down.  |     |     |     |     |     | ACM, el6 (1973) pp. 370-372.  |     |     |     |     |
| ------------ | --- | --- | --- | --- | --- | ----------------------------- | --- | --- | --- | --- |
With  respcct  to  paging  behavior  in  a  virtual  memory  [l)eutsch & Bobrow 76] Dcutsch, L. P., Bobrow, D. G., "An
| environment,  | we  would  | like  to  | compare  | the  following  | three  |     |     |     |     |     |
| ------------- | ---------- | --------- | -------- | --------------- | ------ | --- | --- | --- | --- | --- |
efficient,i ncremental, real-time garbage collector."
execution  strategies:  Communications of the ACM, October 1976.
301

Goldberg 83 Goldberg, A., Robson. I)., "Smalltalk-80: The
i.anguage and its Implementation." Addison-Wesley, Reading,
MA, 1983.
Goldberg 84 Goldberg. A., "Smalltalk-80: The Interactive
Programming Environment." Addison-Wesley, Reading, MA,
L984.
Krasner83 Krasner, Glenn. F'd., "Smalltalk-80: Bits of History,
Words of Advice." Addison-Wesley, Reading, MA, 1983.
.ampson 81 i.ampson, B. W., Ed., "The I)orado: A ! ligh-
Perfi,'mance Personal Computer." Xerox PARC Report CSL-SL-I,
Palo Alto, CA, January 1981.
Mitchell 71 Mitchell, J. G., "The Design and Construction of
Flexible and t:fficient Interactive Programming Systems," Ph.I).
dissertation. 1971, NTIS AI) 712-721, in Outstanding I)i:~sertations
in the Computer Sciences, Garland Publishing, New York (1978).
Moon 73 Moon D., Ed., Maclisp Manual pp. 3-75 to 3-77, MIT AI
Laboratnry Technical Report (1973).
Moore 74 Ml~ore,C . H., "FORTH: a New Way to Program a
Computer." Astronomy and Astrophysics Supplement, # L5 (1974)
pp 497-511.
Patters~m 83 Patterson, D., F.d., "Smalltalk on a RISC:
Architectural Investigations (ProceedingsofCS 292R)." University
of California. Berkeley, April 1983.
Perkins 79 Perkins, I). R., Sites, R. I.., "Machine independent
Pascal code optimization." ACM SIGPI.AN Notices v14 #8
(August 1979) pp. 201-207.
Pittman 80 Pittman, T.J., "A Practical Optimizer: Zero-Address to
Multi-AddressCode." M.S. thesis, University of California, Santa
Cruz, June 1980.
Rau 78 Rau. B. R.. "Levels of Representation of Programs and the
Architecture of Universal Host Machines." Prececdings of Micro-
11, Asilomar, CA. November 1978.
Richards 75 Richards, M., "The portability of the BCPI.
compiler.'" Software, Practice and Experience vl (1971) pp. 135-
146.
SCG 81 Software Concepts Group, special issue on SmaUtalk.
BY'I3-; Magazine, volume 6, number 8, August 1981.
XSIS 83 Masinter, 1.. M., Ed., "Intedisp Reference Manual,"
Xerox Special Inforrnation Systems, Pasadena, CA, 1983.
Zellweger 79 Zellweger, P. T., "Machine-lndependent
Optimization in SOPAIPII.LA.'" The S-! Project 1979 Annual
Report (Chapter 8), i.awrcnce Livermore I.aboratory (1979),
302