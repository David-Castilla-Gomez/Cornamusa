Tracing the Meta-Level: PyPy’s Tracing JIT Compiler
Carl Friedrich Bolz Antonio Cuni
UniversityofDüsseldorf UniversityofGenova
STUPSGroup DISI
Germany Italy
cfbolz@gmx.de cuni@disi.unige.it
Maciej Fijalkowski Armin Rigo
merlinuxGmbH arigo@tunes.org
fijal@merlinux.eu
ABSTRACT stand, extend and port whereas writing a just-in-time com-
pilerisanerror-pronetaskthatismadeevenharderbythe
We attempt to apply the technique of Tracing JIT Com-
dynamic features of a language.
pilers in the context of the PyPy project, i.e., to programs
A recent approach to getting better performance for dy-
thatareinterpretersforsomedynamiclanguages,including
namic languages is that of tracing JIT compilers [16, 7].
Python. Tracing JIT compilers can greatly speed up pro-
Writing a tracing JIT compiler is relatively simple. It can
grams that spend most of their time in loops in which they
beaddedtoanexistinginterpreterforalanguage,theinter-
take similar code paths. However, applying an unmodified
preter takes over some of the functionality of the compiler
tracingJITtoaprogramthatisitselfabytecodeinterpreter
and the machine code generation part can be simplified.
resultsinverylimitedornospeedup. Inthispaperweshow
ThePyPyprojectistryingtofindapproachestogenerally
how to guide tracing JIT compilers to greatly improve the
easetheimplementationofdynamiclanguages. Itstartedas
speed of bytecode interpreters. One crucial point is to un-
aPythonimplementationinPython,buthasnowextended
rollthebytecodedispatchloop,basedontwohintsprovided
its goals to be generally useful for implementing other dy-
by the implementer of the bytecode interpreter. We evalu-
namic languages as well. The general approach is to imple-
ate our technique by applying it to two PyPy interpreters:
ment an interpreter for the language in a subset of Python.
oneisasmallexample,andtheotheroneisthefullPython
This subset is chosen in such a way that programs in it
interpreter.
can be compiled into various target environments, such as
C/Posix,theCLIortheJVM.ThePyPyprojectisdescribed
1. INTRODUCTION in more detail in Section 2.
Dynamic languages have seen a steady rise in popularity InthispaperwediscussongoingworkinthePyPyproject
inrecentyears. JavaScriptisincreasinglybeingusedtoim- toimprovetheperformanceofinterpreterswrittenwiththe
plement full-scale applications which run within a browser, helpofthePyPytoolchain. Theapproachisthatofatrac-
whereasotherdynamiclanguages(suchasRuby,Perl,Python, ingJITcompiler. ContrarytothetracingJITsfordynamic
PHP)areusedfortheserversideofmanywebsites,aswell languagesthatcurrentlyexist,PyPy’stracingJIToperates
as in areas unrelated to the web. “one level down”, i.e., it traces the execution of the inter-
Oneoftheoften-citeddrawbacksofdynamiclanguagesis preter, as opposed to the execution of the user program.
the performance penalties they impose. Typically they are The fact that the program the tracing JIT compiles is in
slower than statically typed languages. Even though there our case always an interpreter brings its own set of prob-
has been a lot of research into improving the performance lems. We describe tracing JITs and their application to in-
of dynamic languages (in the SELF project, to name just terpreters in Section 3. By this approach we hope to arrive
one example [18]), those techniques are not as widely used at a JIT compiler that can be applied to a variety of dy-
as one would expect. Many dynamic language implementa- namic languages, given an appropriate interpreter for each
tions use completely straightforward bytecode-interpreters ofthem. Theprocessisnotcompletelyautomaticbutneeds
without any advanced implementation techniques like just- asmallnumberofhintsfromtheinterpreterauthor,tohelp
in-timecompilation. Thereareanumberofreasonsforthis. the tracing JIT. The details of how the process integrates
Mostofthemboildowntotheinherentcomplexitiesofusing into the rest of PyPy will be explained in Section 4. This
compilation. Interpreters are simple to implement, under- workisnotfinished,buthasalreadyproducedsomepromis-
ing results, which we will discuss in Section 5.
The contributions of this paper are:
• Applying a tracing JIT compiler to an interpreter.
Permissiontomakedigitalorhardcopiesofallorpartofthisworkfor • Finding techniques for improving the generated code.
personalorclassroomuseisgrantedwithoutfeeprovidedthatcopiesare
notmadeordistributedforprofitorcommercialadvantageandthatcopies 2. THEPYPYPROJECT
bearthisnoticeandthefullcitationonthefirstpage.Tocopyotherwise,to
republish,topostonserversortoredistributetolists,requirespriorspecific The PyPy project1 [21, 5] is an environment where flex-
permissionand/orafee.
1http://codespeak.net/pypy
Copyright200XACMX-XXXXX-XX-X/XX/XX...$5.00.

ible implementation of dynamic languages can be written. When a hot loop is identified, the interpreter enters a
ToimplementadynamiclanguagewithPyPy,aninterpreter special mode, called tracing mode. During tracing, the in-
forthatlanguagehastobewritteninRPython[1]. RPython terpreter records a history of all the operations it executes.
(“Restricted Python”) is a subset of Python chosen in such Ittracesuntilithasrecordedtheexecutionofoneiteration
a way that type inference can be performed on it. The lan- of the hot loop. To decide when this is the case, the trace
guage interpreter can then be translated with the help of is repeatedly checked during tracing as to whether the in-
PyPyintovarioustargetenvironments,suchasC/Posix,the terpreter is at a position in the program where it had been
| CLI and | the JVM. | This | is done | by a | component | of PyPy | earlier. |     |     |     |     |     |     |     |
| ------- | -------- | ---- | ------- | ---- | --------- | ------- | -------- | --- | --- | --- | --- | --- | --- | --- |
called the translation toolchain. The history recorded by the tracer is called a trace: it
By writing VMs in a high-level language, we keep the is a sequential list of operations, together with their actual
implementationofthelanguagefreeoflow-leveldetailssuch operands and results. Such a trace can be used to generate
asmemorymanagementstrategy,threadingmodelorobject efficient machine code. This generated machine code is im-
layout. These features are automatically added during the mediatelyexecutable,andcanbeusedinthenextiteration
| translation | process. | The | process | starts | by performing | con- | of the loop. |     |     |     |     |     |     |     |
| ----------- | -------- | --- | ------- | ------ | ------------- | ---- | ------------ | --- | --- | --- | --- | --- | --- | --- |
trol flow graph construction and type inferences, then fol- Beingsequential,thetracerepresentsonlyoneofthemany
lowed by a series of steps, each step transforming the inter- possiblepathsthroughthecode. Toensurecorrectness,the
mediaterepresentationoftheprogramproducedbythepre- trace contains a guard at every possible point where the
vious one until we get the final executable. The first trans- path could have followed another direction, for example at
formation step makes details of the Python object model conditions and indirect or virtual calls. When generating
explicitintheintermediaterepresentation,laterstepsintro- the machine code, every guard is turned into a quick check
ducinggarbagecollectionandotherlow-leveldetails. Aswe to guarantee that the path we are executing is still valid.
will see later, this internal representation of the program is If a guard fails, we immediately quit the machine code and
also used as an input for the tracing JIT. continue the execution by falling back to interpretation.2
|     |     |     |     |     |     |     | It is important |     | to  | understand | how | the tracer | recognizes |     |
| --- | --- | --- | --- | --- | --- | --- | --------------- | --- | --- | ---------- | --- | ---------- | ---------- | --- |
3. TRACINGJITCOMPILERS thatthetraceitrecordedsofarcorrespondstoaloop. This
|              |               |                |      |           |          |            | happens    | when         | the position | key           | is the   | same     | as at an | earlier  |
| ------------ | ------------- | -------------- | ---- | --------- | -------- | ---------- | ---------- | ------------ | ------------ | ------------- | -------- | -------- | -------- | -------- |
| Tracing      | optimizations |                | were | initially | explored | by the Dy- |            |              |              |               |          |          |          |          |
|              |               |                |      |           |          |            | point. The | position     | key          | describes     | the      | position | of       | the exe- |
| namo project | [2]           | to dynamically |      | optimize  | machine  | code at    |            |              |              |               |          |          |          |          |
|              |               |                |      |           |          |            | cution of  | the program, |              | i.e., usually | contains |          | things   | like the |
runtime. Itstechniqueswerethensuccessfullyusedtoimple-
|               |          |      |            |         |              |              | function | currently | being   | executed     | and | the program |             | counter |
| ------------- | -------- | ---- | ---------- | ------- | ------------ | ------------ | -------- | --------- | ------- | ------------ | --- | ----------- | ----------- | ------- |
| ment a JIT    | compiler | for  | a Java     | VM [16, | 15].         | Subsequently |          |           |         |              |     |             |             |         |
|               |          |      |            |         |              |              | position | of the    | tracing | interpreter. | The | tracing     | interpreter |         |
| these tracing | JITs     | were | discovered | to be   | a relatively | simple       |          |           |         |              |     |             |             |         |
doesnotneedtocheckallthetimewhetherthepositionkey
| way to implement |     | JIT compilers |      | for dynamic | languages | [7].   |         |          |              |     |           |              |        |          |
| ---------------- | --- | ------------- | ---- | ----------- | --------- | ------ | ------- | -------- | ------------ | --- | --------- | ------------ | ------ | -------- |
|                  |     |               |      |             |           |        | already | occurred | earlier,     | but | only at   | instructions |        | that are |
| The technique    | is  | now being     | used | by both     | Mozilla’s | Trace- |         |          |              |     |           |              |        |          |
|                  |     |               |      |             |           |        | able to | change   | the position |     | key to an | earlier      | value, | e.g., a  |
MonkeyJavaScriptVM[14]andhasbeentriedforAdobe’s
|     |     |     |     |     |     |     | backward | branch | instruction. |     | Note that | this | is already | the |
| --- | --- | --- | --- | --- | --- | --- | -------- | ------ | ------------ | --- | --------- | ---- | ---------- | --- |
Tamarin ActionScript VM [8]. secondplacewherebackwardbranchesaretreatedspecially:
TracingJITsarebuiltonthefollowingbasicassumptions:
|     |     |     |     |     |     |     | during interpretation |     |     | they are | the place | where | the | profiling |
| --- | --- | --- | --- | --- | --- | --- | --------------------- | --- | --- | -------- | --------- | ----- | --- | --------- |
• programs spend most of their runtime in loops isperformedandwheretracingisstartedoralreadyexisting
|     |     |     |     |     |     |     | assembler | code | executed; | during | tracing | they | are the | place |
| --- | --- | --- | --- | --- | --- | --- | --------- | ---- | --------- | ------ | ------- | ---- | ------- | ----- |
• several iterations of the same loop are likely to take where the check for a closed loop is performed.
similar code paths Let’s look at a small example. Take the (slightly con-
|     |     |     |     |     |     |     | trived) | RPython | code | in Figure | 1.  | The tracer | interprets |     |
| --- | --- | --- | --- | --- | --- | --- | ------- | ------- | ---- | --------- | --- | ---------- | ---------- | --- |
The basic approach of a tracing JIT is to only generate these functions in a bytecode format that is an encoding of
machine code for the hotcode paths of commonly executed the intermediate representation of PyPy’s translation tool-
loopsandtointerprettherestoftheprogram. Thecodefor chain after type inference has been performed. When the
those common loops however is highly optimized, including profilerdiscoversthatthewhileloopinstrange_sumisex-
aggressive inlining. ecutedoftenthetracingJITwillstarttotracetheexecution
| Typically    | tracing    | VMs | go through |     | various | phases when |         |       |           |       |      |           |       |         |
| ------------ | ---------- | --- | ---------- | --- | ------- | ----------- | ------- | ----- | --------- | ----- | ---- | --------- | ----- | ------- |
|              |            |     |            |     |         |             | of that | loop. | The trace | would | look | as in the | lower | half of |
| they execute | a program: |     |            |     |         |             |         |       |           |       |      |           |       |         |
Figure 1.
Theoperationsinthissequenceareoperationsoftheabove-
• Interpretation/profiling
mentionedintermediaterepresentation(e.g.,thegenericmod-
| • Tracing |     |     |     |     |     |     | uloandequalityoperationsinthefunctionabovehavebeen |     |     |     |     |     |     |     |
| --------- | --- | --- | --- | --- | --- | --- | -------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
recognizedtoalwaystakeintegersasargumentsandarethus
• Code generation renderedasint_modandint_eq). Thetracecontainsallthe
|     |     |     |     |     |     |     | operations | that | were | executed | in SSA-form |     | [12] and | ends |
| --- | --- | --- | --- | --- | --- | --- | ---------- | ---- | ---- | -------- | ----------- | --- | -------- | ---- |
• Execution of the generated code with a jump to its beginning, forming an endless loop that
|           |      |             |     |         |            |           | can only | be left | via a     | guard    | failure. The | call | to f is | inlined |
| --------- | ---- | ----------- | --- | ------- | ---------- | --------- | -------- | ------- | --------- | -------- | ------------ | ---- | ------- | ------- |
| At first, | when | the program |     | starts, | everything | is inter- |          |         |           |          |              |      |         |         |
|           |      |             |     |         |            |           | into the | trace.  | The trace | contains | only         | the  | hot     | case    |
preted. The interpreter does a small amount of lightweight else
|           |              |       |       |     |          |             | of the if   | test     | in f, while | the   | other branch |     | is implemented |      |
| --------- | ------------ | ----- | ----- | --- | -------- | ----------- | ----------- | -------- | ----------- | ----- | ------------ | --- | -------------- | ---- |
| profiling | to establish | which | loops | are | run most | frequently. |             |          |             |       |              |     |                |      |
|           |              |       |       |     |          |             | via a guard | failure. | This        | trace | can then     | be  | converted      | into |
Thislightweightprofilingisusuallydonebyhavingacounter
|         |          |      |             |      |        |           | machine | code | and executed. |     |     |     |     |     |
| ------- | -------- | ---- | ----------- | ---- | ------ | --------- | ------- | ---- | ------------- | --- | --- | --- | --- | --- |
| on each | backward | jump | instruction | that | counts | how often |         |      |               |     |     |     |     |     |
thisparticularbackwardjumpisexecuted. Sinceloopsneed 2There are more complex mechanisms in place to still pro-
abackwardjumpsomewhere,thismethodlooksforloopsin duceextracodeforthecasesofguardfailures[15],butthey
the user program. are independent of the issues discussed in this paper.

def f(a, b):
|     | if b % | 46 == | 41: |     |     |     |     |     |     |                     |       |     |     |     |
| --- | ------ | ----- | --- | --- | --- | --- | --- | --- | --- | ------------------- | ----- | --- | --- | --- |
|     | return | a     | - b |     |     |     |     |     | def | interpret(bytecode, |       |     | a): |     |
|     | else:  |       |     |     |     |     |     |     |     | regs =              | [0] * | 256 |     |     |
pc = 0
|     | return | a   | + b |     |     |     |     |     |     |       |       |     |     |     |
| --- | ------ | --- | --- | --- | --- | --- | --- | --- | --- | ----- | ----- | --- | --- | --- |
|     |        |     |     |     |     |     |     |     |     | while | True: |     |     |     |
def strange_sum(n):
|     | result | = 0     |           |     |     |     |     |     |     | opcode | =      | ord(bytecode[pc])   |     |     |
| --- | ------ | ------- | --------- | --- | --- | --- | --- | --- | --- | ------ | ------ | ------------------- | --- | --- |
|     | while  | n >= 0: |           |     |     |     |     |     |     | pc     | += 1   |                     |     |     |
|     | result | =       | f(result, | n)  |     |     |     |     |     | if     | opcode | == JUMP_IF_A:       |     |     |
|     |        |         |           |     |     |     |     |     |     |        | target | = ord(bytecode[pc]) |     |     |
|     | n      | -= 1    |           |     |     |     |     |     |     |        |        |                     |     |     |
|     |        |         |           |     |     |     |     |     |     |        | pc +=  | 1                   |     |     |
|     | return | result  |           |     |     |     |     |     |     |        |        |                     |     |     |
if a:
| # corresponding      |     |     | trace: |     |     |     |     |     |     |      | pc     | = target    |     |     |
| -------------------- | --- | --- | ------ | --- | --- | --- | --- | --- | --- | ---- | ------ | ----------- | --- | --- |
| loop_header(result0, |     |     | n0)    |     |     |     |     |     |     | elif | opcode | == MOV_A_R: |     |     |
n = ord(bytecode[pc])
| i0 =            | int_mod(n0, |                  | Const(46)) |     |     |     |     |     |     |      |                       |                |     |     |
| --------------- | ----------- | ---------------- | ---------- | --- | --- | --- | --- | --- | --- | ---- | --------------------- | -------------- | --- | --- |
|                 |             |                  |            |     |     |     |     |     |     |      | pc +=                 | 1              |     |     |
| i1 =            | int_eq(i0,  |                  | Const(41)) |     |     |     |     |     |     |      |                       |                |     |     |
| guard_false(i1) |             |                  |            |     |     |     |     |     |     |      | regs[n]               | = a            |     |     |
| result1         | =           | int_add(result0, |            | n0) |     |     |     |     |     | elif | opcode                | == MOV_R_A:    |     |     |
| n1 =            | int_sub(n0, |                  | Const(1))  |     |     |     |     |     |     |      | n = ord(bytecode[pc]) |                |     |     |
|                 |             |                  |            |     |     |     |     |     |     |      | pc +=                 | 1              |     |     |
| i2 =            | int_ge(n1,  |                  | Const(0))  |     |     |     |     |     |     |      |                       |                |     |     |
| guard_true(i2)  |             |                  |            |     |     |     |     |     |     |      | a = regs[n]           |                |     |     |
| jump(result1,   |             | n1)              |            |     |     |     |     |     |     | elif | opcode                | == ADD_R_TO_A: |     |     |
n = ord(bytecode[pc])
|     |     |     |     |     |     |     |     |     |     |     | pc += | 1   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | ----- | --- | --- | --- |
a += regs[n]
| Figure   | 1:  | A      | simple | Python |     | function | and | the |     |      |        |            |     |     |
| -------- | --- | ------ | ------ | ------ | --- | -------- | --- | --- | --- | ---- | ------ | ---------- | --- | --- |
|          |     |        |        |        |     |          |     |     |     | elif | opcode | == DECR_A: |     |     |
| recorded |     | trace. |        |        |     |          |     |     |     |      |        |            |     |     |
a -= 1
|     |     |     |     |     |     |     |     |     |     | elif | opcode | == RETURN_A: |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | ---- | ------ | ------------ | --- | --- |
|     |     |     |     |     |     |     |     |     |     |      | return | a            |     |     |
3.1 ApplyingaTracingJITtoanInterpreter
| The         | tracing             | JIT    | of the   | PyPy     | project     | is        | atypical    | in that    |           |         |        |              |                      |      |
| ----------- | ------------------- | ------ | -------- | -------- | ----------- | --------- | ----------- | ---------- | --------- | ------- | ------ | ------------ | -------------------- | ---- |
|             |                     |        |          |          |             |           |             |            | Figure    | 2:      | A very | simple       | bytecode interpreter | with |
| itis        | notappliedtotheuser |        |          | program, |             | but tothe | interpreter |            |           |         |        |              |                      |      |
|             |                     |        |          |          |             |           |             |            | registers |         | and an | accumulator. |                      |      |
| running     | the                 | user   | program. | In       | this        | section   | we will     | explore    |           |         |        |              |                      |      |
| what        | problems            | this   | brings,  | and      | suggest     | how       | to solve    | them       |           |         |        |              |                      |      |
| (at         | least partially).   |        | This     | means    | that        | there     | are two     | inter-     |           |         |        |              |                      |      |
| preters     | involved,           |        | and we   | need     | appropriate |           | terminology | to         |           |         |        |              |                      |      |
| distinguish |                     | beween | them.    | On       | the one     | hand,     | there       | is the in- |           |         |        |              |                      |      |
|             |                     |        |          |          |             |           |             |            |           | MOV_A_R | 0      | # i          | = a                  |      |
terpreterthatthetracingJITusestoperformtracing. This MOV_A_R 1 # copy of ’a’
| wewillcallthetracinginterpreter. |     |     |     |     | Ontheotherhand,there |     |     |     |     |     |     |     |     |     |
| -------------------------------- | --- | --- | --- | --- | -------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
# 4:
| is the | interpreter |          | that         | runs the | user’s | programs,      | which | we      |     |         |     |       |     |     |
| ------ | ----------- | -------- | ------------ | -------- | ------ | -------------- | ----- | ------- | --- | ------- | --- | ----- | --- | --- |
|        |             |          |              |          |        |                |       |         |     | MOV_R_A | 0   | # i-- |     |     |
| will   | call the    | language | interpreter. |          | In     | the following, |       | we will |     |         |     |       |     |     |
DECR_A
| assumethatthelanguageinterpreterisbytecode-based. |      |     |          |             |     |          |     | The       |     |         |     |     |     |     |
| ------------------------------------------------- | ---- | --- | -------- | ----------- | --- | -------- | --- | --------- | --- | ------- | --- | --- | --- | --- |
|                                                   |      |     |          |             |     |          |     |           |     | MOV_A_R | 0   |     |     |     |
| program                                           | that | the | language | interpreter |     | executes | we  | will call |     |         |     |     |     |     |
the user program (from the point of view of a VM author, MOV_R_A 2 # res += a
| the“user”is |             | a programmer |         | using          | the   | VM).              |          |           |     | ADD_R_TO_A | 1   |      |              |     |
| ----------- | ----------- | ------------ | ------- | -------------- | ----- | ----------------- | -------- | --------- | --- | ---------- | --- | ---- | ------------ | --- |
| Similarly,  |             | we           | need to | distinguish    |       | loops             | at two   | different |     | MOV_A_R    | 2   |      |              |     |
| levels:     | interpreter |              | loops   | areloopsinside |       | thelanguageinter- |          |           |     |            |     |      |              |     |
|             |             |              |         |                |       |                   |          |           |     | MOV_R_A    | 0   | # if | i!=0: goto 4 |     |
| preter.     | On          | the other    | hand,   | user           | loops | are               | loops in | the user  |     |            |     |      |              |     |
|             |             |              |         |                |       |                   |          |           |     | JUMP_IF_A  | 4   |      |              |     |
program.
AtracingJITcompilerfindsthehotloopsoftheprogram MOV_R_A 2 # return res
| it is        | compiling. |          | In our         | case,     | this program |             | is the       | language  |        | RETURN_A |         |           |         |            |
| ------------ | ---------- | -------- | -------------- | --------- | ------------ | ----------- | ------------ | --------- | ------ | -------- | ------- | --------- | ------- | ---------- |
| interpreter. |            | The      | most important |           | hot          | interpreter | loop         | is the    |        |          |         |           |         |            |
| bytecode     |            | dispatch | loop           | (for many | simple       |             | interpreters | it is     |        |          |         |           |         |            |
|              |            |          |                |           |              |             |              |           | Figure | 3:       | Example | bytecode: | Compute | the square |
| also         | the only   | hot      | loop).         | Tracing   | one          | iteration   | of           | this loop |        |          |         |           |         |            |
of the accumulator
| means                                     | that    | the recorded |         | trace      | corresponds    |          | to execution | of       |     |     |     |     |     |     |
| ----------------------------------------- | ------- | ------------ | ------- | ---------- | -------------- | -------- | ------------ | -------- | --- | --- | --- | --- | --- | --- |
| one                                       | opcode. | This         | means   | that       | the assumption |          | made         | by the   |     |     |     |     |     |     |
| tracing                                   | JIT     | – that       | several | iterations |                | of a hot | loop         | take the |     |     |     |     |     |     |
| sameorsimilarcodepaths–iswronginthiscase. |         |              |         |            |                |          |              | Itisvery |     |     |     |     |     |     |
unlikely that the same particular opcode is executed many loop_start(a0, regs0, bytecode0, pc0)
| times                                     | in a    | row.     |             |           |          |          |             |           | opcode0              | =              | strgetitem(bytecode0, |           | pc0) |     |
| ----------------------------------------- | ------- | -------- | ----------- | --------- | -------- | -------- | ----------- | --------- | -------------------- | -------------- | --------------------- | --------- | ---- | --- |
|                                           |         |          |             |           |          |          |             |           | pc1                  | = int_add(pc0, |                       | Const(1)) |      |     |
| An                                        | example | is       | given       | in Figure | 2.       | It shows | the code    | of a      |                      |                |                       |           |      |     |
|                                           |         |          |             |           |          |          |             |           | guard_value(opcode0, |                |                       | Const(7)) |      |     |
| very                                      | simple  | bytecode | interpreter |           | with     | 256      | registers   | and an    |                      |                |                       |           |      |     |
|                                           |         |          |             |           |          |          |             |           | a1 =                 | int_sub(a0,    |                       | Const(1)) |      |     |
| accumulator.                              |         | The      | bytecode    |           | argument | is a     | string      | of bytes, |                      |                |                       |           |      |     |
|                                           |         |          |             |           |          |          |             |           | jump(a1,             | regs0,         | bytecode0,            |           | pc1) |     |
| allregisterandtheaccumulatorareintegers.3 |         |          |             |           |          |          | Aprogramfor |           |                      |                |                       |           |      |     |
3Thechainof if,elif,... instructionscheckingthevarious Figure 4: Trace when executing the DECR_A opcode
opcodesisturnedintoaswitchstatementbyoneofPyPy’s
| optimizations. |     | Python |     | does not | have | a switch | statement. |     |     |     |     |     |     |     |
| -------------- | --- | ------ | --- | -------- | ---- | -------- | ---------- | --- | --- | --- | --- | --- | --- | --- |

tlrjitdriver = JitDriver(greens = [’pc’, ’bytecode’],
thisinterpreterthatcomputesthesquareoftheaccumulator
reds = [’a’, ’regs’])
is shown in Figure 3. If the tracing interpreter traces the
execution of the DECR_A opcode (whose integer value is 7), def interpret(bytecode, a):
the trace would look as in Figure 4. Because of the guard regs = [0] * 256
onopcode0,thecodecompiledfromthistracewillbeuseful pc = 0
only when executing a long series of DECR_A opcodes. For while True:
tlrjitdriver.jit_merge_point(
alltheotheroperationstheguardwillfail,whichwillmean
bytecode=bytecode, pc=pc,
that performance is probably not improved at all.
a=a, regs=regs)
Toimprovethissituation,thetracingJITcouldtracethe opcode = ord(bytecode[pc])
execution of several opcodes, thus effectively unrolling the pc += 1
bytecode dispatch loop. Ideally, the bytecode dispatch loop if opcode == JUMP_IF_A:
shouldbeunrolledexactlysomuchthattheunrolledversion target = ord(bytecode[pc])
pc += 1
corresponds to a user loop. User loops occur when the pro-
if a:
gramcounterofthelanguageinterpreter hasthesamevalue
if target < pc:
several times. This program counter is typically stored in tlrjitdriver.can_enter_jit(
one or several variables in the language interpreter, for ex- bytecode=bytecode, pc=target,
amplethebytecodeobjectofthecurrentlyexecutedfunction a=a, regs=regs)
oftheuserprogramandthepositionofthecurrentbytecode pc = target
elif opcode == MOV_A_R:
within that. In the example above, the program counter is
... # rest unmodified
represented by the bytecode and pc variables.
SincethetracingJITcannotknowwhichpartsofthelan-
guageinterpreteraretheprogramcounter,theauthorofthe Figure 5: Simple bytecode interpreter with hints
languageinterpreterneedstomarktherelevantvariablesof applied
the language interpreter with the help of a hint. The trac-
ing interpreter will then effectively add the values of these
variables to the position key. This means that the loop will
to instantiate JitDriver by listing all the variables of the
only be considered to be closed if these variables that are
bytecode loop. The variables are classified into two groups,
makinguptheprogramcounteratthelanguageinterpreter
“green”variables and“red”variables. The green variables
level are the same a second time. Loops found in this way
are those that the tracing JIT should consider to be part
are, by definition, user loops.
of the program counter of the language interpreter. In the
Theprogramcounterofthelanguageinterpretercanonly
case of the example, the pc variable is obviously part of
be the same a second time after an instruction in the user
theprogramcounter;however,thebytecodevariableisalso
program sets it to an earlier value. This happens only at
counted as green, since the pc variable is meaningless with-
backward jumps in the language interpreter. That means
out the knowledge of which bytecode string is currently be-
thatthetracinginterpreterneedstocheckforaclosedloop
ing interpreted. All other variables are red (the fact that
only when it encounters a backward jump in the language
redvariablesneedtobelistedexplicitlytooisanimplemen-
interpreter. Again the tracing JIT cannot know which part
tation detail).
of the language interpreter implements backward jumps, so
Inadditiontotheclassificationofthevariables,thereare
theauthorofthelanguageinterpreterneedstoindicatethis
two methods of JitDriver that need to be called. Both of
with the help of a hint.
them receive as arguments the current values of the vari-
The language interpreter uses a similar technique to de-
ables listed in the definition of the driver. The first one is
tect hot user loops: the profiling is done at the backward
jit_merge_pointwhichneedstobeputatthebeginningof
branches of the user program, using one counter per seen
the body of the bytecode dispatch loop. The other, more
program counter of the language interpreter.
interestingone,iscan_enter_jit. Thismethodneedstobe
The condition for reusing already existing machine code
called at the end of any instruction that can set the pro-
also needs to be adapted to this new situation. In a clas-
gramcounterofthelanguageinterpretertoanearliervalue.
sical tracing JIT there is at most one piece of assembler
FortheexamplethisisonlytheJUMP_IF_Ainstruction,and
code per loop of the jitted program, which in our case is
onlyifitisactuallyabackwardjump. Hereiswherethelan-
the language interpreter. When applying the tracing JIT
guageinterpreterperformsprofilingtodecidewhentostart
to the language interpreter as described so far, all pieces of
tracing. It is also the place where the tracing JIT checks
assemblercodecorrespondtothebytecodedispatchloopof
whether a loop is closed. This is considered to be the case
the language interpreter. However, they correspond to dif-
when the values of the“green”variables are the same as at
ferent paths through the loop and different ways to unroll
an earlier call to the can_enter_jit method.
it. To ascertain which of them to use when trying to enter
For the small example the hints look like a lot of work.
assembler code again, the program counter of the language
However,thenumberofhintsisessentiallyconstantnomat-
interpreterneedstobechecked. Ifitcorrespondstothepo-
terhowlargetheinterpreteris,whichmakestheextrawork
sition key of one of the pieces of assembler code, then this
negligible for larger interpreters.
assemblercodecanbeexecuted. Thischeckagainonlyneeds
WhenexecutingtheSquarefunctionofFigure3,thepro-
to be performed at the backward branches of the language
filingwillidentifytheloopinthesquarefunctiontobehot,
interpreter.
and start tracing. It traces the execution of the interpreter
Let’s look at how hints would need to be applied to the
running the loop of the square function for one iteration,
exampleinterpreterfromFigure2. Figure5showstherele-
thusunrollingtheinterpreterloopoftheexampleinterpreter
vant parts of the interpreter with hints applied. One needs
eight times. The resulting trace can be seen in Figure 6.

|     |     |     |     |     | 3.2 ImprovingtheResult |         |     |            |     |           |     |          |
| --- | --- | --- | --- | --- | ---------------------- | ------- | --- | ---------- | --- | --------- | --- | -------- |
|     |     |     |     |     | The critical           | problem |     | of tracing | the | execution | of  | just one |
opcodehasbeensolved,theloopcorrespondsexactlytothe
|     |     |     |     |     | loop in | the square | function. |     | However, | the | resulting | trace |
| --- | --- | --- | --- | --- | ------- | ---------- | --------- | --- | -------- | --- | --------- | ----- |
loop_start(a0, regs0, bytecode0, pc0) is not optimized enough. Most of its operations are not
# MOV_R_A 0 actually doing any computation that is part of the square
opcode0 = strgetitem(bytecode0, pc0) function. Instead, they manipulate the data structures of
pc1 = int_add(pc0, Const(1)) thelanguageinterpreter. Whilethisistobeexpected,given
| guard_value(opcode0, | Const(2)) |     |     |     |     |     |     |     |     |     |     |     |
| -------------------- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
n1 = strgetitem(bytecode0, pc1) that the tracing interpreter looks at the execution of the
pc2 = int_add(pc1, Const(1)) language interpreter, it would still be an improvement if
a1 = call(Const(<* fn list_getitem>), regs0, n1) some of these operations could be removed.
# DECR_A The simple insight on how to improve the situation is
| opcode1 =            | strgetitem(bytecode0, | pc2) |     |     |              |                                         |            |              |          |            |          |          |
| -------------------- | --------------------- | ---- | --- | --- | ------------ | --------------------------------------- | ---------- | ------------ | -------- | ---------- | -------- | -------- |
|                      |                       |      |     |     | that most    | of the                                  | operations | in           | the      | trace are  | actually | con-     |
| pc3 = int_add(pc2,   | Const(1))             |      |     |     |              |                                         |            |              |          |            |          |          |
|                      |                       |      |     |     | cerned with  | manipulating                            |            | the          | bytecode | string     | and      | the pro- |
| guard_value(opcode1, | Const(7))             |      |     |     |              |                                         |            |              |          |            |          |          |
|                      |                       |      |     |     | gramcounter. | Thosearestoredinvariablesthatare“green” |            |              |          |            |          |          |
| a2 = int_sub(a1,     | Const(1))             |      |     |     |              |                                         |            |              |          |            |          |          |
|                      |                       |      |     |     | (i.e., they  | are                                     | part of    | the position |          | key). This | means    | that     |
| # MOV_A_R            | 0                     |      |     |     |              |                                         |            |              |          |            |          |          |
opcode1 = strgetitem(bytecode0, pc3) thetracerchecksthatthosevariableshavesomefixedvalue
pc4 = int_add(pc3, Const(1)) atthebeginningoftheloop(theymaywellchangeoverthe
guard_value(opcode1, Const(1)) course of the loop, though). In the example of Figure 6
| n2 = strgetitem(bytecode0, | pc4)      |     |     |     |           |       |         |        |           |     |     |           |
| -------------------------- | --------- | --- | --- | --- | --------- | ----- | ------- | ------ | --------- | --- | --- | --------- |
|                            |           |     |     |     | the check | would | be that | at the | beginning | of  | the | trace the |
| pc5 = int_add(pc4,         | Const(1)) |     |     |     |           |       |         |        |           |     |     |           |
call(Const(<* fn list_setitem>), regs0, n2, a2) pc variable is 4 and the bytecode variable is the bytecode
# MOV_R_A 2 string corresponding to the square function. Therefore it
opcode2 = strgetitem(bytecode0, pc5) is possible to constant-fold computations on them away, as
pc6 = int_add(pc5, Const(1)) long as the operations are side-effect free. Since strings are
guard_value(opcode2, Const(2)) immutable in RPython, it is possible to constant-fold the
| n3 = strgetitem(bytecode0, | pc6)               |     |            |     |                |            |        |            |         |               |         |        |
| -------------------------- | ------------------ | --- | ---------- | --- | -------------- | ---------- | ------ | ---------- | ------- | ------------- | ------- | ------ |
|                            |                    |     |            |     | strgetitem     | operation. |        | The        | int_add | are additions |         | of the |
| pc7 = int_add(pc6,         | Const(1))          |     |            |     |                |            |        |            |         |               |         |        |
|                            |                    |     |            |     | green variable |            | pc and | a constant | number, |               | so they | can be |
| a3 = call(Const(<*         | fn list_getitem>), |     | regs0, n3) |     |                |            |        |            |         |               |         |        |
|                            |                    |     |            |     | folded away    | as         | well.  |            |         |               |         |        |
| # ADD_R_TO_A               | 1                  |     |            |     |                |            |        |            |         |               |         |        |
opcode3 = strgetitem(bytecode0, pc7) Withthisoptimizationenabled,thetracelooksasinFig-
pc8 = int_add(pc7, Const(1)) ure7. Nowmuchofthelanguageinterpreterisactuallygone
| guard_value(opcode3,       | Const(5)) |     |     |     |                             |       |          |         |                         |     |              |     |
| -------------------------- | --------- | --- | --- | --- | --------------------------- | ----- | -------- | ------- | ----------------------- | --- | ------------ | --- |
|                            |           |     |     |     | from the                    | trace | and what | is left | corresponds             |     | very closely | to  |
| n4 = strgetitem(bytecode0, | pc8)      |     |     |     |                             |       |          |         |                         |     |              |     |
|                            |           |     |     |     | theloopofthesquarefunction. |       |          |         | Theonlyvestigeofthelan- |     |              |     |
| pc9 = int_add(pc8,         | Const(1)) |     |     |     |                             |       |          |         |                         |     |              |     |
i0 = call(Const(<* fn list_getitem>), regs0, n4) guageinterpreteristhefactthattheregisterlistisstillused
a4 = int_add(a3, i0) tostorethestateofthecomputation. Thiscouldberemoved
# MOV_A_R 2 bysomeotheroptimization,butismaybenotreallyallthat
opcode4 = strgetitem(bytecode0, pc9) bad anyway (in fact we have an experimental optimization
pc10 = int_add(pc9, Const(1)) that does exactly that, but it is not yet finished). Once we
| guard_value(opcode4,       | Const(1))          |        |         |     |                        |     |                   |                     |         |     |       |          |
| -------------------------- | ------------------ | ------ | ------- | --- | ---------------------- | --- | ----------------- | ------------------- | ------- | --- | ----- | -------- |
|                            |                    |        |         |     | getthisoptimizedtrace, |     |                   | wecanpassittotheJIT |         |     |       | backend, |
| n5 = strgetitem(bytecode0, | pc10)              |        |         |     |                        |     |                   |                     |         |     |       |          |
|                            |                    |        |         |     | which generates        |     | the corresponding |                     | machine |     | code. |          |
| pc11 = int_add(pc10,       | Const(1))          |        |         |     |                        |     |                   |                     |         |     |       |          |
| call(Const(<*              | fn list_setitem>), | regs0, | n5, a4) |     |                        |     |                   |                     |         |     |       |          |
| # MOV_R_A                  | 0                  |        |         |     |                        |     |                   |                     |         |     |       |          |
opcode5 = strgetitem(bytecode0, pc11) 4. IMPLEMENTATIONISSUES
| pc12 = int_add(pc11, | Const(1)) |     |     |     |     |     |     |     |     |     |     |     |
| -------------------- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
Inthissectionwewilldescribesomeofthepracticalissues
| guard_value(opcode5, | Const(2)) |     |     |     |     |     |     |     |     |     |     |     |
| -------------------- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
whenimplementingtheschemedescribedinthelastsection
| n6 = strgetitem(bytecode0, | pc12)     |     |     |     |         |                                             |     |     |     |     |     |     |
| -------------------------- | --------- | --- | --- | --- | ------- | ------------------------------------------- | --- | --- | --- | --- | --- | --- |
|                            |           |     |     |     | inPyPy. | Inparticularwewilldescribesomeoftheproblems |     |     |     |     |     |     |
| pc13 = int_add(pc12,       | Const(1)) |     |     |     |         |                                             |     |     |     |     |     |     |
a5 = call(Const(<* fn list_getitem>), regs0, n6) of integrating the various parts with each other.
| # JUMP_IF_A          | 4                     |       |     |     |                  |             |                                     |         |        |        |           |     |
| -------------------- | --------------------- | ----- | --- | --- | ---------------- | ----------- | ----------------------------------- | ------- | ------ | ------ | --------- | --- |
|                      |                       |       |     |     | The first        | integration |                                     | problem | is how | to not | integrate | the |
| opcode6 =            | strgetitem(bytecode0, | pc13) |     |     |                  |             |                                     |         |        |        |           |     |
|                      |                       |       |     |     | tracingJITatall. |             | Itispossibletochoosewhenthelanguage |         |        |        |           |     |
| pc14 = int_add(pc13, | Const(1))             |       |     |     |                  |             |                                     |         |        |        |           |     |
interpreteristranslatedtoCwhethertheJITshouldbebuilt
| guard_value(opcode6, | Const(3)) |     |     |     |     |     |     |     |     |     |     |     |
| -------------------- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
target0 = strgetitem(bytecode0, pc14) in or not. If the JIT is not enabled, all the hints that are
pc15 = int_add(pc14, Const(1)) possibly in the interpreter source are just ignored by the
| i1 = int_is_true(a5) |     |     |     |     | translation | process. |     |     |     |     |     |     |
| -------------------- | --- | --- | --- | --- | ----------- | -------- | --- | --- | --- | --- | --- | --- |
guard_true(i1) If the JIT is enabled, things are more interesting. At the
| jump(a5, regs0, | bytecode0, target0)                 |     |     |     |             |                      |     |         |                           |                  |             |        |
| --------------- | ----------------------------------- | --- | --- | --- | ----------- | -------------------- | --- | ------- | ------------------------- | ---------------- | ----------- | ------ |
|                 |                                     |     |     |     | moment      | the JIT              | can | only be | enabled                   | when             | translating | the    |
|                 |                                     |     |     |     | interpreter | to C,                | but | we hope | to lift                   | that restriction |             | in the |
|                 |                                     |     |     |     | future.     | AclassicaltracingJIT |     |         | willinterprettheprogramit |                  |             |        |
| Figure6:        | TracewhenexecutingtheSquarefunction |     |     |     |             |                      |     |         |                           |                  |             |        |
isrunninguntilahotloopisidentified,atwhichpointtrac-
| of Figure | 3, with the corresponding |     | bytecodes | as  |     |     |     |     |     |     |     |     |
| --------- | ------------------------- | --- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
comments. ingandultimatelyassemblergenerationstarts. Thetracing
JITinPyPyisoperatingonthelanguageinterpreter,which
|     |     |     |     |     | iswritteninRPython.    |     |     | ButRPythonprogramsarestatically |     |     |     |     |
| --- | --- | --- | --- | --- | ---------------------- | --- | --- | ------------------------------- | --- | --- | --- | --- |
|     |     |     |     |     | translatabletoCanyway. |     |     | Thismeansthatinterpretingthe    |     |     |     |     |
languageinterpreterbeforeahotloopisfoundisclearlynot
|     |     |     |     |     | desirable, | since | the overhead |     | of this | double-interpretation |     |     |
| --- | --- | --- | --- | --- | ---------- | ----- | ------------ | --- | ------- | --------------------- | --- | --- |

loop_start(a0, regs0)
point where it is easy to let the C version of the language
# MOV_R_A 0
interpreter resume its operation.4 This means that the fall-
a1 = call(Const(<* fn list_getitem>), regs0, Const(0))
# DECR_A backinterpreterexecutesatmostonebytecodeoperationof
a2 = int_sub(a1, Const(1)) thelanguageinterpreterandthenfallsbacktotheCversion
# MOV_A_R 0 ofthelanguageinterpreter. Afterthis,thewholeprocessof
call(Const(<* fn list_setitem>), regs0, Const(0), a2) profiling may start again.
# MOV_R_A 2
Machinecodeproductionisdoneviaawell-definedinter-
a3 = call(Const(<* fn list_getitem>), regs0, Const(2))
facetoanassemblerbackend. Thisallowseasyportingofthe
# ADD_R_TO_A 1
i0 = call(Const(<* fn list_getitem>), regs0, Const(1)) tracing JIT to various architectures (including, we hope, to
a4 = int_add(a3, i0) virtualmachinessuchastheJVMwhereourbackendcould
# MOV_A_R 2 generate JVM bytecode at runtime). At the moment the
call(Const(<* fn list_setitem>), regs0, Const(2), a4) only implemented backend is a 32-bit Intel-x86 backend.
# MOV_R_A 0
a5 = call(Const(<* fn list_getitem>), regs0, Const(0))
# JUMP_IF_A 4 5. EVALUATION
i1 = int_is_true(a5)
guard_true(i1) Inthissectionweevaluatetheworkdonesofarbylooking
jump(a5, regs0) at some benchmarks. Since the work is not finished, these
can only be preliminary. Benchmarking was done on an
otherwiseidlemachinewitha1.4GHzPentiumMprocessor
Figure7: TracewhenexecutingtheSquarefunction and1GBRAM,usingLinux2.6.27. Allbenchmarkswhere
ofFigure3, withthecorrespondingopcodesascom- run50times,eachinanewlystartedprocess. Thefirstrun
ments. The constant-folding of operations on green wasignored. Thefinalnumberswerereachedbycomputing
variables is enabled. the average of all other runs, the confidence intervals were
computed using a 95% confidence level. All times include
running the tracer and producing machine code.
would be significantly too big to be practical.
The first round of benchmarks (Figure 8) are timings of
What is done instead is that the language interpreter
the example interpreter given in Figure 2 computing the
keeps running as a C program, until a hot loop in the user square of 10000000 using the bytecode of Figure 3.5 The
program is found. To identify loops, the C version of the
results for various configurations are as follows:
language interpreter is generated in such a way that at the
Benchmark 1: The interpreter translated to C without
place that corresponds to the can_enter_jit hint profil-
including a JIT compiler.
ingisperformedusingtheprogramcounterofthelanguage
Benchmark 2: ThetracingJITisenabled,butnointer-
interpreter. Apart from this bit of profiling, the language
preter-specific hints are applied. This corresponds to the
interpreter behaves in just the same way as without a JIT.
traceinFigure4. Thethresholdwhentoconsideraloopto
Whenahotuserloopisidentified,tracingisstarted. The
be hot is 40 iterations. As expected, this is not faster than
tracing interpreter is invoked to start tracing the language
the previous number. It is even quite a bit slower, proba-
interpreter that is running the user program. Of course the
bly due to the overheads, as well as non-optimal generated
tracing interpreter cannot actually trace the execution of
machine code.
the C representation of the language interpreter. Instead it
Benchmark 3: The tracing JIT is enabled and hints as
takes the state of the execution of the language interpreter
inFigure5areapplied. Thismeansthattheinterpreterloop
and starts tracing using a bytecode representation of the
is unrolled so that it corresponds to the loop in the square
language interpreter. That means there are two“versions”
function. Constant folding of green variables is disabled,
ofthelanguageinterpreterembeddedinthefinalexecutable
thereforetheresultingmachinecodecorrespondstothetrace
oftheVM:ontheonehanditisthereasexecutablemachine
in Figure 6. This alone brings an improvement over the
code, on the other hand as bytecode for the tracing inter-
previous case, but is still slower than pure interpretation.
preter. It also means that tracing is costly as it incurs a
Benchmark4: Sameasbefore,butwithconstantfolding
double interpretation overhead.
enabled. This corresponds to the trace in Figure 7. This
From then on things proceed as described in Section 3.
speedsupthesquarefunctionconsiderably,makingitnearly
The tracing interpreter tries to find a loop in the user pro-
three times faster than the pure interpreter.
gram, if it finds one it will produce machine code for that
Benchmark 5: Same as before, but with the threshold
loop and this machine code will be immediately executed.
setsohighthatthetracerisneverinvoked. Inthiswaythe
The machine code is executed until a guard fails. Then the
overhead of the profiling is measured. For this interpreter
execution should fall back to normal interpretation by the
it seems to be rather large, with about 20% slowdown due
languageinterpreter. Thisfallingbackispossiblyacomplex
toprofiling. Thisisbecausetheinterpreterissmallandthe
process,sincetheguardfailurecanhaveoccurredarbitrarily
opcodessimple. Forlargerinterpreters(e.g.,PyPy’sPython
deepinahelperfunctionofthelanguageinterpreter,which
interpreter) the overhead will likely be less significant.
wouldmakeithardtorebuildthestateofthelanguageinter-
Totestthetechniqueonamorerealisticexample,wedid
preterandletitrunfromthatpoint(e.g.,thiswouldinvolve
some preliminary benchmarks with PyPy’s Python inter-
buildingapotentiallydeepCstack). Insteadthefallingback
preter. The function we benchmarked as well as the results
is achieved by a special fallback interpreter which runs the
canbeseeninFigure9. Whilethefunctionmayseemabit
language interpreter and the user program from the point
oftheguardfailure. Thefallbackinterpreterisessentiallya 4This is the only reason for the jit_merge_point hint.
variantofthetracinginterpreterthatdoesnotkeepatrace. 5The result will overflow, but for smaller numbers the run-
The fallback interpreter runs until execution reaches a safe ning time is not long enough to sensibly measure it.

Time (ms) speedup isthatofpartialevaluation[13,19]. Conceptuallythereare
1 Compiled to C, no JIT 442.7 ± 3.4 1.00 somesimilaritiestoourwork. Inpartialevaluationsomear-
2 Normal Trace Compilation 1518.7 ± 7.2 0.29 gumentsoftheinterpreterfunctionareknown(static)while
| 3 Unrolling |      | of Interp.    | Loop |     | 737.6 ± | 7.9 | 0.60 |          |             |     |            |      |            |     |          |
| ----------- | ---- | ------------- | ---- | --- | ------- | --- | ---- | -------- | ----------- | --- | ---------- | ---- | ---------- | --- | -------- |
|             |      |               |      |     |         |     |      | the rest | are unknown |     | (dynamic). | This | separation |     | of argu- |
| 4 JIT,      | Full | Optimizations |      |     | 156.2 ± | 3.8 | 2.83 |          |             |     |            |      |            |     |          |
mentsisrelatedtoourseparationofvariablesintothosethat
| 5 Profile | Overhead |     |     |     | 515.0 ± | 7.2 | 0.86 |           |      |        |          |         |     |          |         |
| --------- | -------- | --- | --- | --- | ------- | --- | ---- | --------- | ---- | ------ | -------- | ------- | --- | -------- | ------- |
|           |          |     |     |     |         |     |      | should be | part | of the | position | key and | the | rest. In | partial |
evaluationallpartsoftheinterpreterthatrelyonlyonstatic
Figure 8: Benchmark results of example interpreter argumentscanbeconstant-foldedsothatonlyoperationson
computing the square of 10000000 the dynamic arguments remain.
|     |     |     |     |     |     |     |     | Classical | partial | evaluation |     | has failed | to  | be useful | for dy- |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ------- | ---------- | --- | ---------- | --- | --------- | ------- |
def f(a): namic language for much the same reasons why ahead-of-
| t = | (1, 2, | 3)  |     |     |     |     |     |                                                |     |     |     |     |     |     |       |
| --- | ------ | --- | --- | --- | --- | --- | --- | ---------------------------------------------- | --- | --- | --- | --- | --- | --- | ----- |
|     |        |     |     |     |     |     |     | timecompilerscannotcompilethemtoefficientcode. |     |     |     |     |     |     | Ifthe |
i = 0
partialevaluatorknowsonlytheprogramitsimplydoesnot
| while | i < | a:  |     |     |     |     |     |     |     |     |     |     |     |     |     |
| ----- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
t = (t[1], t[2], t[0]) have enough information to produce good code. Therefore
|        | i += t[0] |     |     |     |     |     |     | someworkhasbeendonetodopartialevaluationatruntime. |     |     |     |     |     |     |     |
| ------ | --------- | --- | --- | --- | --- | --- | --- | -------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
| return | i         |     |     |     |     |     |     |                                                    |     |     |     |     |     |     |     |
OneoftheearliestworksonruntimespecialisationisTempo
|           |                |         |          |     |       |         |         | forC[10,9].    | However,itisessentiallyanormalpartialeval- |                |                 |           |         |              |         |
| --------- | -------------- | ------- | -------- | --- | ----- | ------- | ------- | -------------- | ------------------------------------------ | -------------- | --------------- | --------- | ------- | ------------ | ------- |
|           |                |         |          |     | Time  | (s)     | speedup |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | uator“packaged |                                            | as a library”; |                 | decisions | about   | what         | can be  |
| 1 PyPy    | compiled       | to      | C, no    | JIT | 23.44 | ± 0.07  | 1.00    |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | specialised    | and                                        | how, are       | pre-determined. |           |         | Another      | work in |
| 2 PyPy    | comp’d         | to      | C, with  | JIT | 3.58  | ± 0.05  | 6.54    |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | this direction |                                            | is DyC         | [17], another   |           | runtime | specializer  | for     |
| 3 CPython |                | 2.5.2   |          |     | 4.96  | ± 0.05  | 4.73    |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | C. Both        | of these                                   | projects       | have            | a problem |         | similar to   | that of |
| 4 CPython |                | 2.5.2 + | Psyco    | 1.6 | 1.51  | ± 0.05  | 15.57   |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | DynamoRIO.     |                                            | Targeting      | the C           | language  | makes   | higher-level |         |
|           |                |         |          |     |       |         |         | specialisation |                                            | difficult.     |                 |           |         |              |         |
| Figure    | 9: Benchmarked |         | function |     | and   | results | for the |                |                                            |                |                 |           |         |              |         |
|           |                |         |          |     |       |         |         | There          | have                                       | been some      | attempts        |           | to do   | dynamic      | partial |
| Python    | interpreter    |         | running  |     |       |         |         |                |                                            |                |                 |           |         |              |         |
f(10000000)
|     |     |     |     |     |     |     |     | evaluation, | which | is partial |     | evaluation | that | defers | partial |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------- | ----- | ---------- | --- | ---------- | ---- | ------ | ------- |
evaluationcompletelytoruntimetomakepartialevaluation
arbitrary,executingitisstillnon-trivial,asanormalPython more useful for dynamic languages. This concept was in-
|                |             |                |                |                |        |             |            | troduced        | by Sullivan |               | [23] who            | implemented |            | it for       | a small  |
| -------------- | ----------- | -------------- | -------------- | -------------- | ------ | ----------- | ---------- | --------------- | ----------- | ------------- | ------------------- | ----------- | ---------- | ------------ | -------- |
| interpreter    | needs       | to dynamically |                | dispatch       |        | nearly      | all of the |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | dynamic         | language    | based         | on lambda-calculus. |             |            | It is        | also re- |
| involved       | operations, | like           | indexing       | into           | the    | tuple,      | addition   |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | lated to        | Psyco       | [20], a       | specializing        | JIT         | compiler   | for          | Python.  |
| and comparison |             | of i.          | We benchmarked |                | PyPy’s | Python      | in-        |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | There is        | some        | work by       | one of              | the authors |            | to implement | a        |
| terpreter      | with        | the JIT        | disabled,      | with           | the    | JIT enabled | and        |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | dynamic         | partial     | evaluator     | for                 | Prolog      | [3]. There | are          | also ex- |
| CPython6       | 2.5.2       | (the reference |                | implementation |        | of          | Python).   |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | periments       | within      | the           | PyPy project        |             | to use     | dynamic      | partial  |
| In addition    | we          | benchmarked    |                | CPython        | using  | Psyco       | 1.6 [20],  |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | evaluation      | for         | automatically |                     | generating  | JIT        | compilers    | out      |
| a specializing | JIT         | compiler       | for            | Python.        |        |             |            |                 |             |               |                     |             |            |              |          |
|                |             |                |                |                |        |             |            | of interpreters |             | [22, 11].     | So far              | those       | have       | not been     | as suc-  |
TheresultsshowthatthetracingJITspeedsuptheexecu-
tion of this Python function significantly, even outperform- cessfulaswewouldlikeanditseemslikelythattheywillbe
ingCPython. Toachievethis,thetracertracesthroughthe supplanted with the work on tracing JITs described here.
| whole Python |     | dispatching | machinery, |     | automatically |     | inlin- |     |     |     |     |     |     |     |     |
| ------------ | --- | ----------- | ---------- | --- | ------------- | --- | ------ | --- | --- | --- | --- | --- | --- | --- | --- |
7. CONCLUSIONANDNEXTSTEPS
| ing the | relevant | fast paths. |     | However, | the | manually | tuned |     |     |     |     |     |     |     |     |
| ------- | -------- | ----------- | --- | -------- | --- | -------- | ----- | --- | --- | --- | --- | --- | --- | --- | --- |
Psyco still performs a lot better than our prototype (al- Wehaveshowntechniquesforimprovingtheresultswhen
though it is interesting to note that Psyco improves the applying a tracing JIT to an interpreter. Our first bench-
speed of CPython by only a factor of 3.29 in this example, marks indicate that these techniques work really well on
while our tracing JIT improves PyPy by a factor of 6.54). smallinterpretersandfirstexperimentswithPyPy’sPython
interpretermakeitappearlikelythattheycanbescaledup
| 6. RELATEDWORK |     |             |           |     |       |             |     | to realistic | examples. |          |     |             |     |             |     |
| -------------- | --- | ----------- | --------- | --- | ----- | ----------- | --- | ------------ | --------- | -------- | --- | ----------- | --- | ----------- | --- |
|                |     |             |           |     |       |             |     | A lot        | of work   | remains. | We  | are working |     | on two main | op- |
| Applying       | a   | trace-based | optimizer |     | to an | interpreter | and |              |           |          |     |             |     |             |     |
adding hints to help the tracer produce better results has timizations at the moment. Those are:
|           |        |       |         |        |           |     |         | Allocation |     | Removal: | A   | key optimization |     | for | making |
| --------- | ------ | ----- | ------- | ------ | --------- | --- | ------- | ---------- | --- | -------- | --- | ---------------- | --- | --- | ------ |
| beentried | before | inthe | context | of the | DynamoRIO |     | project |            |     |          |     |                  |     |     |        |
theapproachproducegoodcodeformorecomplexdynamic
| [24], which | has | been a | great | inspiration | for | our work. | They |     |     |     |     |     |     |     |     |
| ----------- | --- | ------ | ----- | ----------- | --- | --------- | ---- | --- | --- | --- | --- | --- | --- | --- | --- |
languageistoperformescapeanalysisontheloopoperation
achievethesameunrollingoftheinterpreterloopsothatthe
|          |         |             |     |        |       |        |           | aftertracinghasbeenperformed. |     |     |     | Inthiswayallobjectsthat |     |     |     |
| -------- | ------- | ----------- | --- | ------ | ----- | ------ | --------- | ----------------------------- | --- | --- | --- | ----------------------- | --- | --- | --- |
| unrolled | version | corresponds |     | to the | loops | in the | user pro- |                               |     |     |     |                         |     |     |     |
areallocatedduringtheloopanddonotactuallyescapethe
gram. Howevertheapproachisgreatlyhinderedbythefact
loopdonotneedtobeallocatedontheheapatallbutcanbe
| that they  | trace       | on the | machine   | code  | level | and thus     | have no |                                    |     |     |     |     |                      |     |     |
| ---------- | ----------- | ------ | --------- | ----- | ----- | ------------ | ------- | ---------------------------------- | --- | --- | --- | --- | -------------------- | --- | --- |
|            |             |        |           |       |       |              |         | explodedintotheirrespectivefields. |     |     |     |     | Thisisveryhelpfulfor |     |     |
| high-level | information |        | available | about | the   | interpreter. | This    |                                    |     |     |     |     |                      |     |     |
dynamiclanguageswhereprimitivetypesareoftenboxed,as
makesitnecessarytoaddquitealargenumberofhints,be-
therepeatedallocationofintermediateresultsisverycostly.
| cause at   | the assembler |     | level  | it is not     | really | visible | anymore  |            |     |       |          |     |             |     |          |
| ---------- | ------------- | --- | ------ | ------------- | ------ | ------- | -------- | ---------- | --- | ----- | -------- | --- | ----------- | --- | -------- |
|            |               |     |        |               |        |         |          | Optimizing |     | Frame | Objects: |     | One problem |     | with the |
| that e.g., | a bytecode    |     | string | is immutable. |        | Also    | more ad- |            |     |       |          |     |             |     |          |
vanced optimizations like allocation removal would not be removal of allocations is that many dynamic languages are
|          |           |           |     |     |     |     |     | so reflective | that | they | allow the | introspection |     | of the | frame |
| -------- | --------- | --------- | --- | --- | --- | --- | --- | ------------- | ---- | ---- | --------- | ------------- | --- | ------ | ----- |
| possible | with that | approach. |     |     |     |     |     |               |      |      |           |               |     |        |       |
objectthattheinterpreterusestostorelocalvariables(e.g.,
Thestandardapproachforautomaticallyproducingacom-
|     |     |     |     |     |     |     |     | SmallTalk, | Python). |     | This means | that | intermediate |     | results |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------- | -------- | --- | ---------- | ---- | ------------ | --- | ------- |
pilerforaprogramminglanguagegivenaninterpreterforit
alwaysescapebecausetheyarestoredintotheframeobject,
6http://python.org renderingtheallocationremovaloptimizationineffective. To

remedythisproblemwemakeitpossibletoupdatetheframe Proceedings of the 23rd ACM SIGPLAN-SIGACT
object lazily only when it is actually accessed from outside Symposium on Principles of Programming Languages,
of the code generated by the JIT. pages 145–156, St. Petersburg Beach, Florida, United
Furthermore both tracing and leaving machine code are States, 1996. ACM.
very slow due to a double interpretation overhead and we [11] A. Cuni, D. Ancona, and A. Rigo. Faster than C#:
might need techniques for improving those. Efficient implementation of dynamic languages on
Eventually we will need to apply the JIT to the vari- .NET. Submitted to ICOOOLPS’09.
ous interpreters that are written in RPython to evaluate [12] R. Cytron, J. Ferrante, B. K. Rosen, M. N. Wegman,
how widely applicable the described techniques are. Possi- and F. K. Zadeck. Efficiently computing static single
| ble targets | for such       | an evaluation |            | would  | be the       | SPy-VM, a |            |              |                |                |            |                |        |     |
| ----------- | -------------- | ------------- | ---------- | ------ | ------------ | --------- | ---------- | ------------ | -------------- | -------------- | ---------- | -------------- | ------ | --- |
|             |                |               |            |        |              |           | assignment |              | form           | and the        | control    | dependence     | graph. |     |
| Smalltalk   | implementation |               | [4]; a     | Prolog | interpreter; | PyGirl,   |            |              |                |                |            |                |        |     |
|             |                |               |            |        |              |           | ACM        | Transactions |                | on Programming |            | Languages      |        | and |
| a Gameboy   | emulator       | [6];          | and also   | not    | immediately  | obvious   |            |              |                |                |            |                |        |     |
|             |                |               |            |        |              |           | Systems,   |              | 13(4):451–490, |                | Oct. 1991. |                |        |     |
| ones, like  | Python’s       | regular       | expression |        | engine.      |           |            |              |                |                |            |                |        |     |
|             |                |               |            |        |              |           | [13] Y.    | Futamura.    | Partial        | evaluation     |            | of computation |        |     |
If these experiments are successful we hope that we can process - an approach to a Compiler-Compiler.
reach a point where it becomes unnecessary to write a lan- Higher-Order and Symbolic Computation,
guage specificJIT compiler andinsteadpossible tojustap- 12(4):381–391, 1999.
| ply a couple     | of  | hints to        | the interpreter |        | to get  | reasonably |         |           |              |            |             |           |            |     |
| ---------------- | --- | --------------- | --------------- | ------ | ------- | ---------- | ------- | --------- | ------------ | ---------- | ----------- | --------- | ---------- | --- |
|                  |     |                 |                 |        |         |            | [14] A. | Gal, B.   | Eich,        | M. Shaver, | D.          | Anderson, | B. Kaplan, |     |
| good performance |     | with relatively |                 | little | effort. |            |         |           |              |            |             |           |            |     |
|                  |     |                 |                 |        |         |            | G.      | Hoare,    | D. Mandelin, |            | B. Zbarsky, | J.        | Orendorff, |     |
|                  |     |                 |                 |        |         |            | M.      | Bebenita, | M.           | Chang,     | M. Franz,   | E.        | Smith,     |     |
8. REFERENCES
|     |     |     |     |     |     |     | R. Reitmaier, |     | and | M. Haghighat. |     | Trace-based |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------------- | --- | --- | ------------- | --- | ----------- | --- | --- |
[1] D. Ancona, M. Ancona, A. Cuni, and N. D. Matsakis. Just-in-Time type specialization for dynamic
|            |           |              |             |            |             |              | languages. |         | In PLDI,   | 2009.       |           |         |        |     |
| ---------- | --------- | ------------ | ----------- | ---------- | ----------- | ------------ | ---------- | ------- | ---------- | ----------- | --------- | ------- | ------ | --- |
| RPython:   | a         | step towards | reconciling |            | dynamically | and          |            |         |            |             |           |         |        |     |
|            |           |              |             |            |             |              | [15] A.    | Gal and | M. Franz.  | Incremental |           | dynamic | code   |     |
| statically | typed     | OO           | languages.  | In         | Proceedings | of the       |            |         |            |             |           |         |        |     |
|            |           |              |             |            |             |              | generation |         | with trace | trees.      | Technical |         | Report |     |
| 2007       | Symposium | on           | Dynamic     | Languages, |             | pages 53–64, |            |         |            |             |           |         |        |     |
Montreal, Quebec, Canada, 2007. ACM. ICS-TR-06-16,DonaldBrenSchoolofInformationand
[2] V. Bala, E. Duesterwald, and S. Banerjia. Dynamo: a Computer Science, University of California, Irvine,
| transparent |     | dynamic | optimization |     | system. | ACM | Nov. | 2006. |     |     |     |     |     |     |
| ----------- | --- | ------- | ------------ | --- | ------- | --- | ---- | ----- | --- | --- | --- | --- | --- | --- |
SIGPLAN Notices, 35(5):1–12, 2000. [16] A. Gal, C. W. Probst, and M. Franz. HotpathVM: an
effectiveJITcompilerforresource-constraineddevices.
| [3] C. F.                   | Bolz.   | Automatic   | JIT          | Compiler     | Generation   | with |                |           |     |               |               |       |            |     |
| --------------------------- | ------- | ----------- | ------------ | ------------ | ------------ | ---- | -------------- | --------- | --- | ------------- | ------------- | ----- | ---------- | --- |
|                             |         |             |              |              |              |      | In Proceedings |           | of  | the 2nd       | International |       | Conference | on  |
| Runtime                     | Partial | Evaluation. |              | Master       | thesis,      |      |                |           |     |               |               |       |            |     |
|                             |         |             |              |              |              |      | Virtual        | Execution |     | Environments, |               | pages | 144–153,   |     |
| Heinrich-Heine-Universit¨at |         |             |              | Du¨sseldorf, | 2008.        |      |                |           |     |               |               |       |            |     |
|                             |         |             |              |              |              |      | Ottawa,        | Ontario,  |     | Canada,       | 2006.         | ACM.  |            |     |
| [4] C. F.                   | Bolz,   | A. Kuhn,    | A. Lienhard, |              | N. Matsakis, |      |                |           |     |               |               |       |            |     |
O. Nierstrasz, L. Renggli, A. Rigo, and T. Verwaest. [17] B. Grant, M. Mock, M. Philipose, C. Chambers, and
Back to the Future in One Week — Implementing a S. J. Eggers. DyC: an expressive annotation-directed
Smalltalk VM in PyPy, pages 123–139. 2008. dynamiccompiler for c. Theoretical Computer Science,
|           |          |          |     |        |       |           | 248(1–2):147–199, |     |     | 2000. |     |     |     |     |
| --------- | -------- | -------- | --- | ------ | ----- | --------- | ----------------- | --- | --- | ----- | --- | --- | --- | --- |
| [5] C. F. | Bolz and | A. Rigo. | How | to not | write | a virtual |                   |     |     |       |     |     |     |     |
[18] U.H¨olzle.AdaptiveoptimizationforSELF:reconciling
| machine. | In        | Proceedings | of               | the 3rd | Workshop | on     |           |             |         |                  |             |              |       |     |
| -------- | --------- | ----------- | ---------------- | ------- | -------- | ------ | --------- | ----------- | ------- | ---------------- | ----------- | ------------ | ----- | --- |
|          |           |             |                  |         |          |        | high      | performance |         | with exploratory |             | programming. |       |     |
| Dynamic  | Languages |             | and Applications |         | (DYLA    | 2007), |           |             |         |                  |             |              |       |     |
|          |           |             |                  |         |          |        | Technical |             | report, | Stanford         | University, |              | 1994. |     |
2007.
[6] C. Bruni and T. Verwaest. PyGirl: generating [19] N. D. Jones, C. K. Gomard, and P. Sestoft. Partial
Whole-System VMs from High-Level prototypes using evaluation and Automatic Program Generation.
PyPy. In Tools, accepted for publication, 2009. Prentice-Hall, Inc., 1993.
|               |           |                 |      |              |                     |             | [20] A.        | Rigo. Representation-based |            |           |                 | just-in-time |             |     |
| ------------- | --------- | --------------- | ---- | ------------ | ------------------- | ----------- | -------------- | -------------------------- | ---------- | --------- | --------------- | ------------ | ----------- | --- |
| [7] M. Chang, |           | M. Bebenita,    | A.   | Yermolovich, |                     | A. Gal, and |                |                            |            |           |                 |              |             |     |
|               |           |                 |      |              |                     |             | specialization |                            | and        | the psyco | prototype       |              | for python. | In  |
| M. Franz.     | Efficient | Just-In-Time    |      | execution    |                     | of          |                |                            |            |           |                 |              |             |     |
|               |           |                 |      |              |                     |             | Proceedings    |                            | of the     | 2004      | ACM             | SIGPLAN      | Symposium   |     |
| dynamically   |           | typed languages |      | via          | code specialization |             |                |                            |            |           |                 |              |             |     |
|               |           |                 |      |              |                     |             | on             | Partial                    | Evaluation | and       | Semantics-Based |              | Program     |     |
| using         | precise   | runtime         | type | inference.   | Technical           | Report      |                |                            |            |           |                 |              |             |     |
ICS-TR-07-10,DonaldBrenSchoolofInformationand Manipulation, pages 15–26, Verona, Italy, 2004. ACM.
Computer Science, University of California, Irvine, [21] A. Rigo and S. Pedroni. PyPy’s approach to virtual
| 2007.         |               |                   |               |     |              |            | machine     | construction. |             | In  | Companion       |      | to the 21st   | ACM  |
| ------------- | ------------- | ----------------- | ------------- | --- | ------------ | ---------- | ----------- | ------------- | ----------- | --- | --------------- | ---- | ------------- | ---- |
|               |               |                   |               |     |              |            | SIGPLAN     |               | Conference  | on  | Object-Oriented |      |               |      |
| [8] M. Chang, |               | E. Smith,         | R. Reitmaier, |     | M. Bebenita, |            |             |               |             |     |                 |      |               |      |
|               |               |                   |               |     |              |            | Programming |               | Systems,    |     | Languages,      | and  | Applications, |      |
| A. Gal,       | C. Wimmer,    |                   | B. Eich,      | and | M. Franz.    | Tracing    |             |               |             |     |                 |      |               |      |
|               |               |                   |               |     |              |            | pages       | 944–953,      | Portland,   |     | Oregon,         | USA, | 2006.         | ACM. |
| for web       | 3.0:          | Trace compilation |               | for | the next     | generation |             |               |             |     |                 |      |               |      |
|               |               |                   |               |     |              |            | [22] A.     | Rigo and      | S. Pedroni. |     | JIT compiler    |      | architecture. |      |
| web           | applications. | In                | Proceedings   | of  | the 2009     | ACM        |             |               |             |     |                 |      |               |      |
SIGPLAN/SIGOPS International Conference on Technical Report D08.2, PyPy, May 2007.
Virtual Execution Environments, pages 71–80, [23] G. T. Sullivan. Dynamic partial evaluation. In
Washington, DC, USA, 2009. ACM. Proceedings of the Second Symposium on Programs as
[9] C. Consel, L. Hornof, F. No¨el, J. Noy´e, and Data Objects, pages 238–256. Springer-Verlag, 2001.
N. Volanschi. A uniform approach for compile-time [24] G. T. Sullivan, D. L. Bruening, I. Baron, T. Garnett,
|     |          |                 |     |          |         |     | and | S. Amarasinghe. |     | Dynamic |     | native | optimization | of  |
| --- | -------- | --------------- | --- | -------- | ------- | --- | --- | --------------- | --- | ------- | --- | ------ | ------------ | --- |
| and | run-time | specialization. |     | Dagstuhl | Seminar | on  |     |                 |     |         |     |        |              |     |
Partial Evaluation, pages 54—72, 1996. interpreters. In Proceedings of the 2003 Workshop on
[10] C. Consel and F. No¨el. A general approach for Interpreters, Virtual Machines and Emulators, pages
run-time specialization and its application to C. In 50–57, San Diego, California, 2003. ACM.