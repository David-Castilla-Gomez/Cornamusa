| Trace-based |     | Just-in-Time |     |     | Type | Specialization |     |     | for | Dynamic |     |     |
| ----------- | --- | ------------ | --- | --- | ---- | -------------- | --- | --- | --- | ------- | --- | --- |
Languages
+,BrendanEich
|     | AndreasGal |     |     | ,MikeShaver |     | ,DavidAnderson |     | ,DavidMandelin |     |     | ,   |     |
| --- | ---------- | --- | --- | ----------- | --- | -------------- | --- | -------------- | --- | --- | --- | --- |
|     |            | ∗   |     | ∗           |     | ∗              |     | ∗              |     |     | ∗   |     |
MohammadR.Haghighat$,BlakeKaplan ,GraydonHoare ,BorisZbarsky ,JasonOrendorff ,
|     |     |     |     |     | ∗   |     | ∗   |     | ∗   |     | ∗   |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
JesseRuderman ,EdwinSmith#,RickReitmaier#,MichaelBebenita+,MasonChang+#,MichaelFranz+
∗
MozillaCorporation∗
gal,brendan,shaver,danderson,dmandelin,mrbkap,graydon,bz,jorendorff,jruderman @mozilla.com
| {   |     |     |     |     |     |     |     |     |     | }   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
AdobeCorporation#
|     |     |     |     | edwsmith,rreitmai |     | @adobe.com |     |     |     |     |     |     |
| --- | --- | --- | --- | ----------------- | --- | ---------- | --- | --- | --- | --- | --- | --- |
|     |     |     |     | {                 |     | }          |     |     |     |     |     |     |
IntelCorporation$
|     |     |     |     | mohammad.r.haghighat |     | @intel.com |     |     |     |     |     |     |
| --- | --- | --- | --- | -------------------- | --- | ---------- | --- | --- | --- | --- | --- | --- |
|     |     |     | {   |                      |     | }          |     |     |     |     |     |     |
UniversityofCalifornia,Irvine+
|     |     |     |     | mbebenit,changm,franz |     |     | @uci.edu |     |     |     |     |     |
| --- | --- | --- | --- | --------------------- | --- | --- | -------- | --- | --- | --- | --- | --- |
|     |     |     | {   |                       |     | }   |          |     |     |     |     |     |
Abstract
andisusedfortheapplicationlogicofbrowser-basedproductivity
DynamiclanguagessuchasJavaScriptaremoredifficulttocom- applicationssuchasGoogleMail,GoogleDocsandZimbraCol-
pilethanstaticallytypedones.Sincenoconcretetypeinformation laboration Suite. In this domain, in order to provide a fluid user
isavailable,traditionalcompilersneedtoemitgenericcodethatcan experienceandenableanewgenerationofapplications,virtualma-
handleallpossibletypecombinationsatruntime.Wepresentanal- chinesmustprovidealowstartuptimeandhighperformance.
Compilersforstaticallytypedlanguagesrelyontypeinforma-
| ternative compilation | technique | for dynamically-typed |     | languages |     |     |     |     |     |     |     |     |
| --------------------- | --------- | --------------------- | --- | --------- | --- | --- | --- | --- | --- | --- | --- | --- |
tiontogenerateefficientmachinecode.Inadynamicallytypedpro-
thatidentifiesfrequentlyexecutedlooptracesatrun-timeandthen
|                   |         |                 |             |     |         | gramming | language | such | as JavaScript, | the | types of | expressions |
| ----------------- | ------- | --------------- | ----------- | --- | ------- | -------- | -------- | ---- | -------------- | --- | -------- | ----------- |
| generates machine | code on | the fly that is | specialized | for | the ac- |          |          |      |                |     |          |             |
mayvaryatruntime.Thismeansthatthecompilercannolonger
tualdynamictypesoccurringoneachpaththroughtheloop.Our
easilytransformoperationsintomachineinstructionsthatoperate
methodprovidescheapinter-proceduraltypespecialization,andan
ononespecifictype.Withoutexacttypeinformation,thecompiler
elegantandefficientwayofincrementallycompilinglazilydiscov-
mustemitslowergeneralizedmachinecodethatcandealwithall
eredalternativepathsthroughnestedloops.Wehaveimplemented
potentialtypecombinations.Whilecompile-timestatictypeinfer-
adynamiccompilerforJavaScriptbasedonourtechniqueandwe
|               |             |              |             |           |     | ence  | might be | able to gather    | type   | information | to generate | opti-     |
| ------------- | ----------- | ------------ | ----------- | --------- | --- | ----- | -------- | ----------------- | ------ | ----------- | ----------- | --------- |
| have measured | speedups of | 10x and more | for certain | benchmark |     |       |          |                   |        |             |             |           |
|               |             |              |             |           |     | mized | machine  | code, traditional | static | analysis    | is very     | expensive |
programs.
andhencenotwellsuitedforthehighlyinteractiveenvironmentof
| Categories | and Subject Descriptors | D.3.4 | [Programming |     | Lan- | awebbrowser. |     |     |     |     |     |     |
| ---------- | ----------------------- | ----- | ------------ | --- | ---- | ------------ | --- | --- | --- | --- | --- | --- |
guages]:Processors—Incrementalcompilers,codegeneration. We present a trace-based compilation technique for dynamic
languagesthatreconcilesspeedofcompilationwithexcellentper-
Design,Experimentation,Measurement,Perfor-
GeneralTerms formanceofthegeneratedmachinecode.Oursystemusesamixed-
mance.
modeexecutionapproach:thesystemstartsrunningJavaScriptina
fast-startingbytecodeinterpreter.Astheprogramruns,thesystem
| Keywords | JavaScript,just-in-timecompilation,tracetrees. |     |     |     |     |            |                 |      |                |          |            |            |
| -------- | ---------------------------------------------- | --- | --- | --- | --- | ---------- | --------------- | ---- | -------------- | -------- | ---------- | ---------- |
|          |                                                |     |     |     |     | identifies | hot (frequently |      | executed)      | bytecode | sequences, | records    |
|          |                                                |     |     |     |     | them,      | and compiles    | them | to fast native | code.    | We call    | such a se- |
1. Introduction
quenceofinstructionsatrace.
DynamiclanguagessuchasJavaScript,Python,andRuby,arepop- Unlike method-based dynamic compilers, our dynamic com-
ularsincetheyareexpressive,accessibletonon-experts,andmake piler operates at the granularity of individual loops. This design
deploymentaseasyasdistributingasourcefile.Theyareusedfor choice is based on the expectation that programs spend most of
small scripts as well as for complex applications. JavaScript, for theirtimeinhotloops.Evenindynamicallytypedlanguages,we
example,isthedefactostandardforclient-sidewebprogramming expecthotloopstobemostlytype-stable,meaningthatthetypesof
valuesareinvariant.(12)Forexample,wewouldexpectloopcoun-
tersthatstartasintegerstoremainintegersforalliterations.When
bothoftheseexpectationshold,atrace-basedcompilercancover
Permissiontomakedigitalorhardcopiesofallorpartofthisworkforpersonalor theprogramexecutionwithasmallnumberoftype-specialized,ef-
classroomuseisgrantedwithoutfeeprovidedthatcopiesarenotmadeordistributed ficientlycompiledtraces.
forprofitorcommercialadvantageandthatcopiesbearthisnoticeandthefullcitation Eachcompiledtracecoversonepaththroughtheprogramwith
onthefirstpage.Tocopyotherwise,torepublish,topostonserversortoredistribute
onemappingofvaluestotypes.WhentheVMexecutesacompiled
tolists,requirespriorspecificpermissionand/orafee.
|     |     |     |     |     |     | trace, | it cannot | guarantee | that the | same | path will | be followed |
| --- | --- | --- | --- | --- | --- | ------ | --------- | --------- | -------- | ---- | --------- | ----------- |
PLDI’09, June15–20,2009,Dublin,Ireland.
Copyright c 2009ACM978-1-60558-392-1/09/06...$5.00 or that the same types will occur in subsequent loop iterations.
￿
465

|     |     |     |     |     |     |     | 1 for (var | i = 2; | i < 100; ++i) | {   |     |
| --- | --- | --- | --- | --- | --- | --- | ---------- | ------ | ------------- | --- | --- |
Hence,recordingandcompilingatracespeculatesthatthepathand
|     |     |     |     |     |     |     | 2 if | (!primes[i]) |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ---- | ------------ | --- | --- | --- |
typingwillbeexactlyastheywereduringrecordingforsubsequent
|     |     |     |     |     |     |     | 3 continue; |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ----------- | --- | --- | --- | --- |
iterationsoftheloop.
|     |     |     |     |     |     |     | 4 for | (var k = i | + i; i < 100; | k += | i)  |
| --- | --- | --- | --- | --- | --- | --- | ----- | ---------- | ------------- | ---- | --- |
Everycompiledtracecontainsalltheguards(checks)required
|                    |                  |            |             |        |                |          | 5 primes[k] | =   | false; |     |     |
| ------------------ | ---------------- | ---------- | ----------- | ------ | -------------- | -------- | ----------- | --- | ------ | --- | --- |
| to validate        | the speculation. | If         | one of the  | guards | fails (if      | control  |             |     |        |     |     |
|                    |                  |            |             |        |                |          | 6 }         |     |        |     |     |
| flow is different, | or               | a value of | a different | type   | is generated), | the      |             |     |        |     |     |
| trace exits.       | If an exit       | becomes    | hot, the    | VM can | record         | a branch |             |     |        |     |     |
tracestartingattheexittocoverthenewpath.Inthisway,theVM Figure 1. Sample program: sieve of Eratosthenes. primes is
recordsatracetreecoveringallthehotpathsthroughtheloop. initialized to an array of 100 false values on entry to this code
| Nested                   | loops can | be difficult   | to optimize | for       | tracing     | VMs. In    | snippet. |     |     |     |     |
| ------------------------ | --------- | -------------- | ----------- | --------- | ----------- | ---------- | -------- | --- | --- | --- | --- |
| a na¨ıve implementation, |           | inner          | loops would | become    | hot         | first, and |          |     |     |     |     |
| the VM would             | start     | tracing there. | When        | the inner | loop exits, | the        |          |     |     |     |     |
VMwoulddetectthatadifferentbranchwastaken.TheVMwould
trytorecordabranchtrace,andfindthatthetracereachesnotthe Symbol Key
Interpret
innerloopheader,buttheouterloopheader.Atthispoint,theVM Overhead
Bytecodes
couldcontinuetracinguntilitreachestheinnerloopheaderagain,
|     |     |     |     |     |     |     |     |     | loop  |     | Interpreting |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ----- | --- | ------------ |
thus tracing the outer loop inside a trace tree for the inner loop. cold/blacklisted
|     |     |     |     |     |     |     |     |     | edge |     | Native |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ---- | --- | ------ |
Butthisrequirestracingacopyoftheouterloopforeverysideexit loop/exit
| andtypecombinationintheinnerloop.Inessence,thisisaform |     |     |     |     |     |     | abort     |     |                          |     |     |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | --- | --------- | --- | ------------------------ | --- | --- |
|                                                        |     |     |     |     |     |     | recording |     | Monitor  compiled trace  |     |     |
ofunintendedtailduplication,whichcaneasilyoverflowthecode ready
hot
cache.Alternatively,theVMcouldsimplystoptracing,andgiveup Record Enter
|     |     |     |     |     |     |     | LIR Trace | loop/exit |     | Compiled Trace |     |
| --- | --- | --- | --- | --- | --- | --- | --------- | --------- | --- | -------------- | --- |
onevertracingouterloops.
finish at
We solve the nested loop problem by recording nested trace loop header loop edge with
same types
trees.Oursystemtracestheinnerloopexactlyasthena¨ıveversion.
Thesystemstopsextendingtheinnertreewhenitreachesanouter Compile Execute
|     |     |     |     |     |     |     | LIR Trace |     |     | Compiled Trace |     |
| --- | --- | --- | --- | --- | --- | --- | --------- | --- | --- | -------------- | --- |
loop,butthenitstartsanewtraceattheouterloopheader.When
theouterloopreachestheinnerloopheader,thesystemtriestocall
side exit,
thetracetreefortheinnerloop.Ifthecallsucceeds,theVMrecords no existing trace side exit to
the call to the inner tree as part of the outer trace and finishes existing trace
Leave
the outer trace as normal. In this way, our system can trace any Compiled Trace
numberofloopsnestedtoanydepthwithoutcausingexcessivetail
duplication.
These techniques allow a VM to dynamically translate a pro- StatemachinedescribingthemajoractivitiesofTrace-
Figure2.
gram to nested, type-specialized trace trees. Because traces can Monkey and the conditions that cause transitions to a new activ-
crossfunctioncallboundaries,ourtechniquesalsoachievetheef- ity. In the dark box, TM executes JS as compiled traces. In the
fectsofinlining.Becausetraceshavenointernalcontrol-flowjoins, lightgrayboxes,TMexecutesJSinthestandardinterpreter.White
they can be optimized in linear time by a simple compiler (10). boxes are overhead. Thus, to maximize performance, we need to
Thus, our tracing VM efficiently performs the same kind of op- maximizetimespentinthedarkestboxandminimizetimespentin
timizations that would require interprocedural analysis in a static thewhiteboxes.Thebestcaseisaloopwherethetypesattheloop
optimizationsetting.Thismakestracinganattractiveandeffective edgearethesameasthetypesonentry–thenTMcanstayinnative
tooltotypespecializeevencomplexfunctioncall-richcode. codeuntiltheloopisdone.
WeimplementedthesetechniquesforanexistingJavaScriptin-
terpreter,SpiderMonkey.WecalltheresultingtracingVMTrace-
Monkey.TraceMonkeysupportsalltheJavaScriptfeaturesofSpi- asetofindustrybenchmarks.Thepaperendswithconclusionsin
derMonkey,witha2x-20xspeedupfortraceableprograms. Section9andanoutlookonfutureworkispresentedinSection10.
Thispapermakesthefollowingcontributions:
|     |     |     |     |     |     |     | 2. Overview:ExampleTracingRun |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ----------------------------- | --- | --- | --- | --- |
Weexplainanalgorithmfordynamicallyformingtracetreesto
•
coveraprogram,representingnestedloopsasnestedtracetrees. This section provides an overview of our system by describing
|     |     |     |     |     |     |     | how TraceMonkey | executes | an example | program. | The example |
| --- | --- | --- | --- | --- | --- | --- | --------------- | -------- | ---------- | -------- | ----------- |
Weexplainhowtospeculativelygenerateefficienttype-specialized
| •   |     |     |     |     |     |     | program,showninFigure1,computesthefirst100primenumbers |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------------------------------------------------------ | --- | --- | --- | --- |
codefortracesfromdynamiclanguageprograms.
withnestedloops.ThenarrativeshouldbereadalongwithFigure2,
Wevalidateourtracingtechniquesinanimplementationbased whichdescribestheactivitiesTraceMonkeyperformsandwhenit
•
ontheSpiderMonkeyJavaScriptinterpreter,achieving2x-20x transitionsbetweentheloops.
speedupsonmanyprograms. TraceMonkey always begins executing a program in the byte-
|     |     |     |     |     |     |     | code interpreter. | Every | loop back edge | is a potential | trace point. |
| --- | --- | --- | --- | --- | --- | --- | ----------------- | ----- | -------------- | -------------- | ------------ |
Theremainderofthispaperisorganizedasfollows.Section3is When the interpreter crosses a loop edge, TraceMonkey invokes
ageneraloverviewoftracetreebasedcompilationweusetocap- thetracemonitor,whichmaydecidetorecordorexecuteanative
ture and compile frequently executed code regions. In Section 4 trace.Atthestartofexecution,therearenocompiledtracesyet,so
we describe our approach of covering nested loops using a num- thetracemonitorcountsthenumberoftimeseachloopbackedgeis
ber of individual trace trees. In Section 5 we describe our trace- executeduntilaloopbecomeshot,currentlyafter2crossings.Note
compilationbasedspeculativetypespecializationapproachweuse thatthewayourloopsarecompiled,theloopedgeiscrossedbefore
togenerateefficientmachinecodefromrecordedbytecodetraces. enteringtheloop,sothesecondcrossingoccursimmediatelyafter
| Our implementation |     | of a dynamic | type-specializing |     | compiler | for | thefirstiteration. |     |     |     |     |
| ------------------ | --- | ------------ | ----------------- | --- | -------- | --- | ------------------ | --- | --- | --- | --- |
JavaScriptisdescribedinSection6.Relatedworkisdiscussedin Here is the sequence of events broken down by outer loop
| Section8.InSection7weevaluateourdynamiccompilerbasedon |     |     |     |     |     |     | iteration: |     |     |     |     |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- |
466

| v0  | := ld      | state[748] | // load    | primes  | from           | the trace   | activation |        | record |
| --- | ---------- | ---------- | ---------- | ------- | -------------- | ----------- | ---------- | ------ | ------ |
|     | st         | sp[0], v0  | // store   | primes  | to             | interpreter | stack      |        |        |
| v1  | := ld      | state[764] | // load    | k       | from the       | trace       | activation | record |        |
| v2  | := i2f(v1) |            | // convert |         | k from         | int to      | double     |        |        |
|     | st         | sp[8], v1  | // store   | k       | to interpreter |             | stack      |        |        |
|     | st         | sp[16], 0  | // store   | false   | to interpreter |             | stack      |        |        |
| v3  | := ld      | v0[4]      | // load    | class   | word           | for primes  |            |        |        |
| v4  | := and     | v3, -4     | // mask    | out     | object         | class       | tag for    | primes |        |
| v5  | := eq      | v4, Array  | // test    | whether | primes         | is          | an array   |        |        |
|     | xf         | v5         | // side    | exit    | if v5          | is false    |            |        |        |
v6 := js_Array_set(v0, v2, false) // call function to set array element
| v7  | := eq | v6, 0 | // test | return | value           | from | call    |     |        |
| --- | ----- | ----- | ------- | ------ | --------------- | ---- | ------- | --- | ------ |
|     | xt    | v7    | // side | exit   | if js_Array_set |      | returns |     | false. |
Figure 3. LIR snippet for sample program. This is the LIR recorded for line 5 of the sample program in Figure 1. The LIR encodes
thesemanticsinSSAformusingtemporaryvariables.TheLIRalsoencodesallthestoresthattheinterpreterwoulddotoitsdatastack.
Sometimesthesestorescanbeoptimizedawayasthestacklocationsareliveonlyonexitstotheinterpreter.Finally,theLIRrecordsguards
andsideexitstoverifytheassumptionsmadeinthisrecording:thatprimesisanarrayandthatthecalltosetitselementsucceeds.
| mov  | edx,         | ebx(748) | // load  | primes   | from             | the trace        | activation   |            | record |
| ---- | ------------ | -------- | -------- | -------- | ---------------- | ---------------- | ------------ | ---------- | ------ |
| mov  | edi(0),      | edx      | // (*)   | store    | primes           | to interpreter   |              | stack      |        |
| mov  | esi,         | ebx(764) | // load  | k from   | the              | trace activation |              | record     |        |
| mov  | edi(8),      | esi      | // (*)   | store    | k to interpreter |                  | stack        |            |        |
| mov  | edi(16),     | 0        | // (*)   | store    | false            | to interpreter   |              | stack      |        |
| mov  | eax,         | edx(4)   | // (*)   | load     | object           | class word       | for          | primes     |        |
| and  | eax,         | -4       | // (*)   | mask     | out object       | class            | tag          | for primes |        |
| cmp  | eax,         | Array    | // (*)   | test     | whether          | primes           | is an        | array      |        |
| jne  | side_exit_1  |          | // (*)   | side     | exit if          | primes           | is not       | an array   |        |
| sub  | esp,         | 8        | // bump  | stack    | for call         | alignment        |              | convention |        |
| push | false        |          | // push  | last     | argument         | for              | call         |            |        |
| push | esi          |          | // push  | first    | argument         | for              | call         |            |        |
| call | js_Array_set |          | // call  | function | to               | set array        | element      |            |        |
| add  | esp,         | 8        | // clean | up       | extra stack      | space            |              |            |        |
| mov  | ecx,         | ebx      | // (*)   | created  | by register      |                  | allocator    |            |        |
| test | eax,         | eax      | // (*)   | test     | return           | value of         | js_Array_set |            |        |
| je   | side_exit_2  |          | // (*)   | side     | exit if          | call failed      |              |            |        |
...
side_exit_1:
| mov | ecx,   | ebp(-4) | // restore |     | ecx           |     |     |     |     |
| --- | ------ | ------- | ---------- | --- | ------------- | --- | --- | --- | --- |
| mov | esp,   | ebp     | // restore |     | esp           |     |     |     |     |
| jmp | epilog |         | // jump    | to  | ret statement |     |     |     |     |
Figure4. x86snippetforsampleprogram.Thisisthex86codecompiledfromtheLIRsnippetinFigure3.MostLIRinstructionscompile
toasinglex86instruction.Instructionsmarkedwith(*)wouldbeomittedbyanidealizedcompilerthatknewthatnoneofthesideexits
wouldeverbetaken.The17instructionsgeneratedbythecompilercomparefavorablywiththe100+instructionsthattheinterpreterwould
executeforthesamecodesnippet,including4indirectjumps.
i=2. This is the first iteration of the outer loop. The loop on interpreterPCandthetypesofvaluesmatchthoseobservedwhen
lines4-5becomeshotonitsseconditeration,soTraceMonkeyen- trace recording was started. The first trace in our example, T 45,
ters recording mode on line 4. In recording mode, TraceMonkey coverslines4and5.ThistracecanbeenteredifthePCisatline4,
recordsthecodealongthetraceinalow-levelcompilerintermedi- iandkareintegers,andprimesisanobject.AftercompilingT 45,
aterepresentationwecallLIR.TheLIRtraceencodesalltheoper- TraceMonkeyreturnstotheinterpreterandloopsbacktoline1.
ationsperformedandthetypesofalloperands.TheLIRtracealso i=3.Nowtheloopheaderatline1hasbecomehot,soTrace-
encodesguards,whicharechecksthatverifythatthecontrolflow Monkey starts recording. When recording reaches line 4, Trace-
and types are identical to those observed during trace recording. Monkeyobservesthatithasreachedaninnerloopheaderthatal-
Thus,onlaterexecutions,ifandonlyifallguardsarepassed,the readyhasacompiledtrace,soTraceMonkeyattemptstonestthe
tracehastherequiredprogramsemantics. innerloopinsidethecurrenttrace.Thefirststepistocalltheinner
TraceMonkey stops recording when execution returns to the traceasasubroutine.Thisexecutesthelooponline4tocompletion
loopheaderorexitstheloop.Inthiscase,executionreturnstothe andthenreturnstotherecorder.TraceMonkeyverifiesthatthecall
loopheaderonline4. wassuccessfulandthenrecordsthecalltotheinnertraceaspartof
Afterrecordingisfinished,TraceMonkeycompilesthetraceto thecurrenttrace.Recordingcontinuesuntilexecutionreachesline
nativecodeusingtherecordedtypeinformationforoptimization. 1,andatwhichpointTraceMonkeyfinishesandcompilesatrace
fortheouterloop,T
The result is a native code fragment that can be entered if the 16
467

i=4.Onthisiteration,TraceMonkeycallsT 16.Becausei=4,the Atracerecordsallitsintermediatevaluesinasmallactivation
ifstatementonline2istaken.Thisbranchwasnottakeninthe recordarea.Tomakevariableaccessesfastontrace,thetracealso
originaltrace,sothiscausesT 16tofailaguardandtakeasideexit. importslocalandglobalvariablesbyunboxingthemandcopying
Theexitisnotyethot,soTraceMonkeyreturnstotheinterpreter, them to its activation record. Thus, the trace can read and write
whichexecutesthecontinuestatement. thesevariableswithsimpleloadsandstoresfromanativeactivation
i=5.TraceMonkeycallsT 16,whichinturncallsthenestedtrace recording, independently of the boxing mechanism used by the
T 45. T 16 loops back to its own header, starting the next iteration interpreter. When the trace exits, the VM boxes the values from
withouteverreturningtothemonitor. thisnativestoragelocationandcopiesthembacktotheinterpreter
i=6.Onthisiteration,thesideexitonline2istakenagain.This structures.
time, the side exit becomes hot, so a trace T 23,1 is recorded that For every control-flow branch in the source program, the
coversline3andreturnstotheloopheader.Thus,theendofT 23,1 recordergeneratesconditionalexitLIRinstructions.Theseinstruc-
jumpsdirectlytothestartofT 16.Thesideexitispatchedsothat tionsexitfromthetraceifrequiredcontrolflowisdifferentfrom
onfutureiterations,itjumpsdirectlytoT 23,1. whatitwasattracerecording,ensuringthatthetraceinstructions
Atthispoint,TraceMonkeyhascompiledenoughtracestocover are run only if they are supposed to. We call these instructions
the entire nested loop structure, so the rest of the program runs guardinstructions.
entirelyasnativecode. Mostofourtracesrepresentloopsandendwiththespecialloop
LIRinstruction.Thisisjustanunconditionalbranchtothetopof
thetrace.Suchtracesreturnonlyviaguards.
3. TraceTrees
Now,wedescribethekeyoptimizationsthatareperformedas
In this section, we describe traces, trace trees, and how they are partofrecordingLIR.Alloftheseoptimizationsreducecomplex
formedatruntime.Althoughourtechniquesapplytoanydynamic dynamic language constructs to simple typed constructs by spe-
language interpreter, we will describe them assuming a bytecode cializingforthecurrenttrace.Eachoptimizationrequiresguardin-
interpretertokeeptheexpositionsimple. structions to verify their assumptions about the state and exit the
traceifnecessary.
3.1 Traces Typespecialization.
All LIR primitives apply to operands of specific types. Thus,
A trace is simply a program path, which may cross function call
LIR traces are necessarily type-specialized, and a compiler can
boundaries.TraceMonkeyfocusesonlooptraces,thatoriginateat
easily produce a translation that requires no type dispatches. A
aloopedgeandrepresentasingleiterationthroughtheassociated
typicalbytecodeinterpretercarriestagbitsalongwitheachvalue,
loop.
andtoperformanyoperation,mustcheckthetagbits,dynamically
Similar to an extended basic block, a trace is only entered at
dispatch, mask out the tag bits to recover the untagged value,
thetop,butmayhavemanyexits.Incontrasttoanextendedbasic
performtheoperation,andthenreapplytags.LIRomitseverything
block, a trace can contain join nodes. Since a trace always only
excepttheoperationitself.
followsonesinglepaththroughtheoriginalprogram,however,join
Apotentialproblemisthatsomeoperationscanproducevalues
nodes are not recognizable as such in a trace and have a single
of unpredictable types. For example, reading a property from an
predecessornodelikeregularnodes.
object could yield a value of any type, not necessarily the type
Atypedtraceisatraceannotatedwithatypeforeveryvariable
observed during recording. The recorder emits guard instructions
(includingtemporaries)onthetrace.Atypedtracealsohasanentry
thatconditionallyexitiftheoperationyieldsavalueofadifferent
typemapgivingtherequiredtypesforvariablesusedonthetrace
type from that seen during recording. These guard instructions
beforetheyaredefined.Forexample,atracecouldhaveatypemap
guaranteethataslongasexecutionisontrace,thetypesofvalues
(x: int, b: boolean),meaningthatthetracemaybeentered
matchthoseofthetypedtrace.WhentheVMobservesasideexit
onlyifthevalueofthevariablexisoftypeintandthevalueofb
alongsuchatypeguard,anewtypedtraceisrecordedoriginating
isoftypeboolean.Theentrytypemapismuchlikethesignature
atthesideexitlocation,capturingthenewtypeoftheoperationin
ofafunction.
question.
In this paper, we only discuss typed loop traces, and we will
Representation specialization: objects. In JavaScript, name
refer to them simply as “traces”. The key property of typed loop
lookup semantics are complex and potentially expensive because
tracesisthattheycanbecompiledtoefficientmachinecodeusing
theyincludefeatureslikeobjectinheritanceandeval.Toevaluate
thesametechniquesusedfortypedlanguages.
an object property read expression like o.x, the interpreter must
InTraceMonkey,tracesarerecordedintrace-flavoredSSALIR
searchthepropertymapofoandallofitsprototypesandparents.
(low-level intermediate representation). In trace-flavored SSA (or
Property maps can be implemented with different data structures
TSSA),phinodesappearonlyattheentrypoint,whichisreached
(e.g., per-object hash tables or shared hash tables), so the search
both on entry and via loop edges. The important LIR primitives
process also must dispatch on the representation of each object
are constant values, memory loads and stores (by address and
foundduringsearch.TraceMonkeycansimplyobservetheresultof
offset), integer operators, floating-point operators, function calls,
thesearchprocessandrecordthesimplestpossibleLIRtoaccess
andconditionalexits.Typeconversions,suchasintegertodouble,
thepropertyvalue.Forexample,thesearchmightfindsthevalueof
are represented by function calls. This makes the LIR used by
o.xintheprototypeofo,whichusesasharedhash-tablerepresen-
TraceMonkey independent of the concrete type system and type
tationthatplacesxinslot2ofapropertyvector.Thentherecorded
conversion rules of the source language. The LIR operations are
cangenerateLIRthatreadso.xwithjusttwoorthreeloads:oneto
genericenoughthatthebackendcompilerislanguageindependent.
gettheprototype,possiblyonetogetthepropertyvaluevector,and
Figure3showsanexampleLIRtrace.
onemoretogetslot2fromthevector.Thisisavastsimplification
Bytecode interpreters typically represent values in a various
andspeedupcomparedtotheoriginalinterpretercode.Inheritance
complexdatastructures(e.g.,hashtables)inaboxedformat(i.e.,
relationshipsandobjectrepresentationscanchangeduringexecu-
withattachedtypetagbits).Sinceatraceisintendedtorepresent
tion,sothesimplifiedcoderequiresguardinstructionsthatensure
efficientcodethateliminatesallthatcomplexity,ourtracesoper-
theobjectrepresentationisthesame.InTraceMonkey,objects’rep-
ate on unboxed values in simple variables and arrays as much as
possible.
468

resentations are assigned an integer key called the object shape. Startingatree.Treetreesalwaysstartatloopheaders,because
Thus,theguardisasimpleequalitycheckontheobjectshape. theyareanaturalplacetolookforhotpaths.InTraceMonkey,loop
Representation specialization: numbers. JavaScript has no headers are easy to detect–the bytecode compiler ensures that a
integer type, only a Number type that is the set of 64-bit IEEE- bytecodeisaloopheaderiffitisthetargetofabackwardbranch.
754 floating-pointer numbers (“doubles”). But many JavaScript TraceMonkeystartsatreewhenagivenloopheaderhasbeenexe-
operators,inparticulararrayaccessesandbitwiseoperators,really cutedacertainnumberoftimes(2inthecurrentimplementation).
operateonintegers,sotheyfirstconvertthenumbertoaninteger, Startingatreejustmeansstartingrecordingatraceforthecurrent
and then convert any integer result back to a double.1 Clearly, a pointandtypemapandmarkingthetraceastherootofatree.Each
JavaScriptVMthatwantstobefastmustfindawaytooperateon treeisassociatedwithaloopheaderandtypemap,sotheremaybe
integersdirectlyandavoidtheseconversions. severaltreesforagivenloopheader.
InTraceMonkey,wesupporttworepresentationsfornumbers: Closingtheloop.Tracerecordingcanendinseveralways.
integers and doubles. The interpreter uses integer representations Ideally,thetracereachestheloopheaderwhereitstartedwith
asmuchasitcan,switchingforresultsthatcanonlyberepresented the same type map as on entry. This is called a type-stable loop
asdoubles.Whenatraceisstarted,somevaluesmaybeimported iteration. In this case, the end of the trace can jump right to the
and represented as integers. Some operations on integers require beginning,asallthevaluerepresentationsareexactlyasneededto
guards.Forexample,addingtwointegerscanproduceavaluetoo enterthetrace.Thejumpcanevenskiptheusualcodethatwould
largefortheintegerrepresentation. copyoutthestateattheendofthetraceandcopyitbackintothe
Function inlining. LIR traces can cross function boundaries traceactivationrecordtoenteratrace.
ineitherdirection,achievingfunctioninlining.Moveinstructions In certain cases the trace might reach the loop header with a
needtoberecordedforfunctionentryandexittocopyarguments differenttypemap.Thisscenarioissometimeobservedforthefirst
inandreturnvaluesout.Thesemovestatementsarethenoptimized iterationofaloop.Somevariablesinsidetheloopmightinitiallybe
awaybythecompilerusingcopypropagation.Inordertobeable undefined,beforetheyaresettoaconcretetypeduringthefirstloop
to return to the interpreter, the trace must also generate LIR to iteration. When recording such an iteration, the recorder cannot
record that a call frame has been entered and exited. The frame linkthetracebacktoitsownloopheadersinceitistype-unstable.
entry and exit LIR saves just enough information to allow the Instead,theiterationisterminatedwithasideexitthatwillalways
intepretercallstacktoberestoredlaterandismuchsimplerthan fail and return to the interpreter. At the same time a new trace is
the interpreter’s standard call code. If the function being entered recorded with the new type map. Every time an additional type-
isnotconstant(whichinJavaScriptincludesanycallbyfunction unstabletraceisaddedtoaregion,itsexittypemapiscomparedto
name),therecordermustalsoemitLIRtoguardthatthefunction theentrymapofallexistingtracesincasetheycomplementeach
isthesame. other.Withthisapproachweareabletocovertype-unstableloop
Guards and side exits. Each optimization described above iterationsaslongtheyeventuallyformastableequilibrium.
requires one or more guards to verify the assumptions made in Finally,thetracemightexittheloopbeforereachingtheloop
doingtheoptimization.AguardisjustagroupofLIRinstructions header,forexamplebecauseexecutionreachesabreakorreturn
that performs a test and conditional exit. The exit branches to a statement.Inthiscase,theVMsimplyendsthetracewithanexit
side exit, a small off-trace piece of LIR that returns a pointer to tothetracemonitor.
a structure that describes the reason for the exit along with the As mentioned previously, we may speculatively chose to rep-
interpreterPCattheexitpointandanyotherdataneededtorestore resentcertainNumber-typedvaluesasintegersontrace.Wedoso
theinterpreter’sstatestructures. when we observe that Number-typed variables contain an integer
Aborts. Some constructs are difficult to record in LIR traces. valueattraceentry.Ifduringtracerecordingthevariableisunex-
For example, eval or calls to external functions can change the pectedlyassignedanon-integervalue,wehavetowidenthetype
program state in unpredictable ways, making it difficult for the ofthevariabletoadouble.Asaresult,therecordedtracebecomes
tracer to know the current type map in order to continue tracing. inherently type-unstable since it starts with an integer value but
Atracingimplementationcanalsohaveanynumberofotherlimi- endswithadoublevalue.Thisrepresentsamis-speculation,since
tations,e.g.,asmall-memorydevicemaylimitthelengthoftraces. attraceentrywespecializedtheNumber-typedvaluetoaninteger,
Whenanysituationoccursthatpreventstheimplementationfrom assumingthatattheloopedgewewouldagainfindanintegervalue
continuingtracerecording,theimplementationabortstracerecord- inthevariable,allowingustoclosetheloop.Toavoidfuturespec-
ingandreturnstothetracemonitor. ulativefailuresinvolvingthisvariable,andtoobtainatype-stable
tracewenotethefactthatthevariableinquestionasbeenobserved
tosometimesholdnon-integervaluesinanadvisorydatastructure
3.2 TraceTrees
whichwecallthe“oracle”.
Especially simple loops, namely those where control flow, value Whencompilingloops,weconsulttheoraclebeforespecializ-
types,valuerepresentations,andinlinedfunctionsareallinvariant, ing values to integers. Speculation towards integers is performed
canberepresentedbyasingletrace.Butmostloopshaveatleast only if no adverse information is known to the oracle about that
some variation, and so the program will take side exits from the particularvariable.Wheneverweaccidentallycompilealoopthat
main trace. When a side exit becomes hot, TraceMonkey starts a is type-unstable due to mis-speculation of a Number-typed vari-
newbranchtracefromthatpointandpatchesthesideexittojump able,weimmediatelytriggertherecordingofanewtrace,which
directlytothattrace.Inthisway,asingletraceexpandsondemand basedonthenowupdatedoracleinformationwillstartwithadou-
toasingle-entry,multiple-exittracetree. blevalueandthusbecometypestable.
Thissectionexplainshowtracetreesareformedduringexecu- Extending a tree. Side exits lead to different paths through
tion.Thegoalistoformtracetreesduringexecutionthatcoverall theloop,orpathswithdifferenttypesorrepresentations.Thus,to
thehotpathsoftheprogram. completelycovertheloop,theVMmustrecordtracesstartingatall
sideexits.Thesetracesarerecordedmuchlikeroottraces:thereis
acounterforeachsideexit,andwhenthecounterreachesahotness
1Arraysareactuallyworsethanthis:iftheindexvalueisanumber,itmust threshold,recordingstarts.Recordingstopsexactlyasfortheroot
beconvertedfromadoubletoastringforthepropertyaccessoperator,and trace,usingtheloopheaderoftheroottraceasthetargettoreach.
thentoanintegerinternallytothearrayimplementation.
469

Ourimplementationdoesnotextendatallsideexits.Itextends
| onlyifthesideexitisforacontrol-flowbranch,andonlyiftheside |     |     |     |     |     |     |     |     | !   |     |     |     |
| ---------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
exitdoesnotleavetheloop.Inparticularwedonotwanttoextend !"))&*$(+,"
a trace tree along a path that leads to an outer loop, because we !"#$%&!"'()
wanttocoversuchpathsinanoutertreethroughtreenesting. !"'()&*$(+,"
-"'$(+&!"'()
3.3 Blacklisting
.#'"/
Sometimes, a program follows a path that cannot be compiled 01/)&2314
intoatrace,usuallybecauseoflimitationsintheimplementation.
| TraceMonkey | does | not currently | support | recording | throwing | and |     |     |     |     |     |     |
| ----------- | ---- | ------------- | ------- | --------- | -------- | --- | --- | --- | --- | --- | --- | --- |
catchingofarbitraryexceptions.Thisdesigntradeoffwaschosen,
| because | exceptions  | are usually | rare in      | JavaScript. | However, | if a     |     |     |     |     |     |     |
| ------- | ----------- | ----------- | ------------ | ----------- | -------- | -------- | --- | --- | --- | --- | --- | --- |
| program | opts to use | exceptions  | intensively, | we          | would    | suddenly |     |     |     |     |     |     |
incurapunishingruntimeoverheadifwerepeatedlytrytorecord
| a trace for | this path | and repeatedly | fail | to do | so, since | we abort |     |     |     |     |     |     |
| ----------- | --------- | -------------- | ---- | ----- | --------- | -------- | --- | --- | --- | --- | --- | --- |
tracingeverytimeweobserveanexceptionbeingthrown. Figure 5. A tree with two traces, a trunk trace and one branch
Asaresult,ifahotloopcontainstracesthatalwaysfail,theVM trace.Thetrunktracecontainsaguardtowhichabranchtracewas
couldpotentiallyrunmuchmoreslowlythanthebaseinterpreter: attached.Thebranchtracecontainaguardthatmayfailandtrigger
theVMrepeatedlyspendstimetryingtorecordtraces,butisnever asideexit.Boththetrunkandthebranchtraceloopbacktothetree
abletorunany.Toavoidthisproblem,whenevertheVMisabout anchor,whichisthebeginningofthetracetree.
tostarttracing,itmusttrytopredictwhetheritwillfinishthetrace.
| Our prediction |     | algorithm | is based | on blacklisting | traces | that |     |     |     |     |     |     |
| -------------- | --- | --------- | -------- | --------------- | ------ | ---- | --- | --- | --- | --- | --- | --- |
havebeentriedandfailed.WhentheVMfailstofinishatracestart- !"#$%&( !"#$%&' !"#$%&( !"#$%&'
ingatagivenpoint,theVMrecordsthatafailurehasoccurred.The
|     |     |     |     |     |     |     |     | 2345%" | 9++*%#0 | 2345%" | 9++*%#0 |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | ------- | ------ | ------- | --- |
VMalsosetsacountersothatitwillnottrytorecordatracestarting
atthatpointuntilitispassedafewmoretimes(32inourimple-
| mentation). | This backoff | counter | gives | temporary | conditions | that |     |     |     |     |     |     |
| ----------- | ------------ | ------- | ----- | --------- | ---------- | ---- | --- | --- | --- | --- | --- | --- |
preventtracingachancetoend.Forexample,aloopmaybehave
|     |     |     |     |     |     |     |     | 2345%" | 2345%" | 9++*%#0 | 2345%" |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | ------ | ------- | ------ | --- |
differentlyduringstartupthanduringitssteady-stateexecution.Af-
|     |     |     |     |     |     |     |     | )*+,%- | ./01%- | ./01%- | ./01%- |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | ------ | ------ | ------ | --- |
teragivennumberoffailures(2inourimplementation),theVM
|     |     |     |     |     |     |     |     |     | ;#< |     | ;5< |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
marksthefragmentasblacklisted,whichmeanstheVMwillnever
| againstartrecordingatthatpoint. |              |      |                 |     |          |          |     | !"#$%&( |     | !"#$%&' | !"#$%&: |     |
| ------------------------------- | ------------ | ---- | --------------- | --- | -------- | -------- | --- | ------- | --- | ------- | ------- | --- |
| After                           | implementing | this | basic strategy, | we  | observed | that for |     |         |     |         |         |     |
smallloopsthatgetblacklisted,thesystemcanspendanoticeable 2345%" 9++*%#0 67"/08
amountoftimejustfindingtheloopfragmentanddeterminingthat
ithasbeenblacklisted.Wenowavoidthatproblembypatchingthe
bytecode.Wedefineanextrano-opbytecodethatindicatesaloop
|                                                        |     |     |     |     |     |     |     |        | 2345%" | 67"/08 | 67"/08 |     |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | --- | --- | ------ | ------ | ------ | ------ | --- |
| header.TheVMcallsintothetracemonitoreverytimetheinter- |     |     |     |     |     |     |     | 67"/08 |        |        |        |     |
preter executes a loop header no-op. To blacklist a fragment, we ./01%- ./01%- )*+,%-
./01%-
| simplyreplacetheloopheaderno-opwitharegularno-op.Thus, |     |     |     |     |     |     |     |     |     | ;$< |     |     |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
theinterpreterwillneveragainevencallintothetracemonitor.
Thereisarelatedproblemwehavenotyetsolved,whichoccurs
|     |     |     |     |     |     |     | Figure | 6. We handle | type-unstable | loops | by allowing | traces to |
| --- | --- | --- | --- | --- | --- | --- | ------ | ------------ | ------------- | ----- | ----------- | --------- |
whenaloopmeetsalloftheseconditions:
|     |     |     |     |     |     |     | compile | that cannot | loop | back to themselves | due to | a type mis- |
| --- | --- | --- | --- | --- | --- | --- | ------- | ----------- | ---- | ------------------ | ------ | ----------- |
TheVMcanformatleastoneroottracefortheloop. match.Assuchtracesaccumulate,weattempttoconnecttheirloop
•
There is at least one hot side exit for which the VM cannot edgestoformgroupsoftracetreesthatcanexecutewithouthaving
• completeatrace. toside-exittotheinterpretertocoveroddtypecases.Thisispar-
ticularlyimportantfornestedtracetreeswhereanoutertreetriesto
Theloopbodyisshort.
• callaninnertree(orinthiscaseaforestofinnertrees),sinceinner
Inthiscase,theVMwillrepeatedlypasstheloopheader,search loopsfrequentlyhaveinitiallyundefinedvalueswhichchangetype
for a trace, find it, execute it, and fall back to the interpreter. toaconcretevalueafterthefirstiteration.
| With a short   | loop       | body, the   | overhead | of finding | and calling | the      |     |     |     |            |     |     |
| -------------- | ---------- | ----------- | -------- | ---------- | ----------- | -------- | --- | --- | --- | ---------- | --- | --- |
| trace is high, | and causes | performance |          | to be even | slower      | than the |     |     |     |            |     |     |
|                |            |             |          |            |             |          |     |     |     | i ,i ,i ,α | α   |     |
basic interpreter. So far, in this situation we have improved the through the inner loop, 2 3 5 . The symbol is used to
|     |     |     |     |     |     |     |     |     | {   | }   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
implementation so that the VM can complete the branch trace. indicatethatthetraceloopsbackthetreeanchor.
Whenexecutionleavestheinnerloop,thebasicdesignhastwo
| But it is | hard to guarantee |     | that this situation |     | will never | happen. |     |     |     |     |     |     |
| --------- | ----------------- | --- | ------------------- | --- | ---------- | ------- | --- | --- | --- | --- | --- | --- |
choices.First,thesystemcanstoptracingandgiveuponcompiling
| As future | work, this | situation | could be | avoided | by detecting | and |     |     |     |     |     |     |
| --------- | ---------- | --------- | -------- | ------- | ------------ | --- | --- | --- | --- | --- | --- | --- |
theouterloop,clearlyanundesirablesolution.Theotherchoiceis
| blacklisting | loops | for which | the average | trace | call executes | few |     |     |     |     |     |     |
| ------------ | ----- | --------- | ----------- | ----- | ------------- | --- | --- | --- | --- | --- | --- | --- |
tocontinuetracing,compilingtracesfortheouterloopinsidethe
bytecodesbeforereturningtotheinterpreter.
innerloop’stracetree.
|     |     |     |     |     |     |     | Forexample,theprogrammightexitati |     |     |     | andrecordabranch |     |
| --- | --- | --- | --- | --- | --- | --- | --------------------------------- | --- | --- | --- | ---------------- | --- |
5
4. NestedTraceTreeFormation trace that incorporates the outer loop: i ,i ,i ,i ,i ,i ,α .
|     |     |     |     |     |     |     |     |     |     |     | 5 7 1 | 6 7 1 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | ----- | ----- |
Figure7showsbasictracetreecompilation(11)appliedtoanested Later, the program might take the other { branch at i and then }
2
loop where the inner loop contains two paths. Usually, the inner exit, recording another branch trace incorporating the outer loop:
loop(withheaderati 2)becomeshotfirst,andatracetreeisrooted i ,i ,i ,i ,i ,i ,i ,i ,α .Thus,theouterloopisrecordedand
|     |     |     |     |     |     |     | 2 4 | 5 7 1 | 6 7 1 |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ----- | ----- | --- | --- | --- |
atthatpoint.Forexample,thefirstrecordedtracemaybeacycle compiledtwice,andbothcopiesmustberetainedinthetracecache. { }
470

|     |     | !&  |     | )&  | 12),+-*+,, |     |     |     |     | !$  | ($  |     |     |     |     |
| --- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
123(20+4/22
*+,,-./00
|     |     |     |     |     |     |     |     |     |     | !"  |     | ("  |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
!"
3,4),5-*+,,
!#
)"
!' !# !$
)*!(+,-./0
|     |     |     |     |     |     |     |     |     |     | !&  |     | (&  |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
!%
!'
67!)-82/+5
!(
!%
|     |     | 9/: |     | 9;: |     |     |     |          |                                                 |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ----------------------------------------------- | --- | --- | --- | --- | --- | --- |
|     |     |     |     |     |     |     |     | Figure8. | Controlflowgraphofaloopwithtwonestedloops(left) |     |     |     |     |     |     |
anditsnestedtracetreeconfiguration(right).Theoutertreecalls
Figure7. Controlflowgraphofanestedloopwithanifstatement thetwoinnernestedtracetreesandplacesguardsattheirsideexit
| inside the | inner most | loop | (a). An | inner | tree captures | the | inner | locations. |     |     |     |     |     |     |     |
| ---------- | ---------- | ---- | ------- | ----- | ------------- | --- | ----- | ---------- | --- | --- | --- | --- | --- | --- | --- |
loop,andisnestedinsideanoutertreewhich“calls”theinnertree.
Theinnertreereturnstotheoutertreeonceitexitsalongitsloop
conditionguard(b).
loopisenteredwithmdifferenttypemaps(ongeometricaverage),
thenwecompileO(mk)copiesoftheinnermostloop.Aslongas
miscloseto1,theresultingtracetreeswillbetractable.
Ingeneral,ifloopsarenestedtodepthk,andeachloophasnpaths
Animportantdetailisthatthecalltotheinnertracetreemustact
| (on geometric | average), | this | na¨ıve | strategy | yields | O(nk) | traces, |     |     |     |     |     |     |     |     |
| ------------- | --------- | ---- | ------ | -------- | ------ | ----- | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
likeafunctioncallsite:itmustreturntothesamepointeverytime.
whichcaneasilyfillthetracecache.
Thegoalofnestingistomakeinnerandouterloopsindependent;
| In order | to execute | programs |     | with nested | loops | efficiently, | a   |           |           |      |            |         |         |          |       |
| -------- | ---------- | -------- | --- | ----------- | ----- | ------------ | --- | --------- | --------- | ---- | ---------- | ------- | ------- | -------- | ----- |
|          |            |          |     |             |       |              |     | thus when | the inner | tree | is called, | it must | exit to | the same | point |
tracingsystemneedsatechniqueforcoveringthenestedloopswith
intheoutertreeeverytimewiththesametypemap.Becausewe
nativecodewithoutexponentialtraceduplication.
cannotactuallyguaranteethisproperty,wemustguardonitafter
|     |     |     |     |     |     |     |     | the call, | and side | exit if | the property | does | not hold. | A common |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | -------- | ------- | ------------ | ---- | --------- | -------- | --- |
4.1 NestingAlgorithm
|     |     |     |     |     |     |     |     | reason for | the inner | tree | not to | return to | the same | point | would |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------- | --------- | ---- | ------ | --------- | -------- | ----- | ----- |
Thekeyinsightisthatifeachloopisrepresentedbyitsowntrace
|     |     |     |     |     |     |     |     | be if the | inner tree | took | a new | side exit | for which | it had | never |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ---------- | ---- | ----- | --------- | --------- | ------ | ----- |
tree,thecodeforeachloopcanbecontainedonlyinitsowntree,
|     |     |     |     |     |     |     |     | compiled | a trace. | At this | point, the | interpreter | PC  | is in the | inner |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | -------- | ------- | ---------- | ----------- | --- | --------- | ----- |
andouterlooppathswillnotbeduplicated.Anotherkeyfactisthat
tree,sowecannotcontinuerecordingorexecutingtheoutertree.
wearenottracingarbitrarybytecodesthatmighthaveirreduceable
Ifthishappensduringrecording,weaborttheoutertrace,togive
controlflowgraphs,butratherbytecodesproducedbyacompiler
theinnertreeachancetofinishgrowing.Afutureexecutionofthe
foralanguagewithstructuredcontrolflow.Thus,giventwoloop
outertreewouldthenbeabletoproperlyfinishandrecordacallto
| edges, the | system | can easily | determine | whether |     | they are | nested |     |     |     |     |     |     |     |     |
| ---------- | ------ | ---------- | --------- | ------- | --- | -------- | ------ | --- | --- | --- | --- | --- | --- | --- | --- |
theinnertree.Ifaninnertreesideexithappensduringexecutionof
andwhichistheinnerloop.Usingthisknowledge,thesystemcan
acompiledtracefortheoutertree,wesimplyexittheoutertrace
compileinnerandouterloopsseparately,andmaketheouterloop’s
andstartrecordinganewbranchintheinnertree.
tracescalltheinnerloop’stracetree.
Thealgorithmforbuildingnestedtracetreesisasfollows.We
|     |     |     |     |     |     |     |     | 4.2 BlacklistingwithNesting |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------------------------- | --- | --- | --- | --- | --- | --- | --- |
starttracingatloopheadersexactlyasinthebasictracingsystem.
When we exit a loop (detected by comparing the interpreter PC The blacklisting algorithm needs modification to work well with
with the range given by the loop edge), we stop the trace. The nesting. The problem is that outer loop traces often abort during
key step of the algorithm occurs when we are recording a trace startup(becausetheinnertreeisnotavailableortakesasideexit),
forloopL (Rforloopbeingrecorded)andwereachtheheader which would lead to their being quickly blacklisted by the basic
R
| ofadifferentloopL |     | O(Oforotherloop).NotethatL |     |     |     | Omustbean |     | algorithm. |     |     |     |     |     |     |     |
| ----------------- | --- | -------------------------- | --- | --- | --- | --------- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
innerloopofL Rbecausewestopthetracewhenweexitaloop. Thekeyobservationisthatwhenanoutertraceabortsbecause
theinnertreeisnotready,thisisprobablyatemporarycondition.
| IfL | hasatype-matchingcompiledtracetree,wecallL |     |     |     |     |     | as  |     |     |     |     |     |     |     |     |
| --- | ------------------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
• O O Thus,weshouldnotcountsuchabortstowardblacklistingaslong
anestedtracetree.Ifthecallsucceeds,thenwerecordthecall
asweareabletobuildupmoretracesfortheinnertree.
| inthetraceforL |     | R.Onfutureexecutions,thetraceforL |     |     |     |     | Rwill |     |     |     |     |     |     |     |     |
| -------------- | --- | --------------------------------- | --- | --- | --- | --- | ----- | --- | --- | --- | --- | --- | --- | --- | --- |
Inourimplementation,whenanoutertreeabortsontheinner
calltheinnertracedirectly.
|     |     |     |     |     |     |     |     | tree, we | increment | the outer | tree’s | blacklist | counter | as usual | and |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | --------- | --------- | ------ | --------- | ------- | -------- | --- |
IfL doesnothaveatype-matchingcompiledtracetreeyet, backoffoncompilingit.Whentheinnertreefinishesatrace,we
• O
we have to obtain it before we are able to proceed. In order decrementtheblacklistcounterontheouterloop,“forgiving”the
todothis,wesimplyabortrecordingthefirsttrace.Thetrace outerloopforabortingpreviously.Wealsoundothebackoffsothat
monitor will see the inner loop header, and will immediately theoutertreecanstartimmediatelytryingtocompilethenexttime
| startrecordingtheinnerloop.2 |     |     |     |     |     |     |     | wereachit. |     |     |     |     |     |     |     |
| ---------------------------- | --- | --- | --- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
Ifalltheloopsinanestaretype-stable,thenloopnestingcreates
noduplication.Otherwise,ifloopsarenestedtoadepthk,andeach
|     |     |     |     |     |     |     |     | 5. TraceTreeOptimization |          |     |            |       |     |            |       |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------------------ | -------- | --- | ---------- | ----- | --- | ---------- | ----- |
|     |     |     |     |     |     |     |     | This section             | explains | how | a recorded | trace | is  | translated | to an |
2Insteadofabortingtheouterrecording,wecouldprincipallymerelysus-
|     |     |     |     |     |     |     |     | optimized | machine | code | trace. The | trace | compilation | subsystem, |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ------- | ---- | ---------- | ----- | ----------- | ---------- | --- |
pendtherecording,butthatwouldrequiretheimplementationtobeable
|     |     |     |     |     |     |     |     | NANOJIT, | is separate | from | the | VM and | can be | used for | other |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ----------- | ---- | --- | ------ | ------ | -------- | ----- |
torecordseveraltracessimultaneously,complicatingtheimplementation,
applications.
whilesavingonlyafewiterationsintheinterpreter.
471

| 5.1 Optimizations |     |     |     |     |     |     | Tag JSType |     | Description |     |     |     |
| ----------------- | --- | --- | --- | --- | --- | --- | ---------- | --- | ----------- | --- | --- | --- |
Because traces are in SSA form and have no join points or φ- xx1 number 31-bitintegerrepresentation
nodes, certain optimizations are easy to implement. In order to 000 object pointertoJSObjecthandle
getgoodstartupperformance,theoptimizationsmustrunquickly, 010 number pointertodoublehandle
so we chose a small set of optimizations. We implemented the 100 string pointertoJSStringhandle
optimizationsaspipelinedfilterssothattheycanbeturnedonand 110 boolean enumerationfornull,undefined,true,false
| offindependently,andyetallruninjusttwolooppassesoverthe |             |          |        |                    |        |         | null,or   |        |        |                     |     |              |
| ------------------------------------------------------- | ----------- | -------- | ------ | ------------------ | ------ | ------- | --------- | ------ | ------ | ------------------- | --- | ------------ |
| trace:oneforwardandonebackward.                         |             |          |        |                    |        |         | undefined |        |        |                     |     |              |
| Every time                                              | the trace   | recorder | emits  | a LIR instruction, |        | the in- |           |        |        |                     |     |              |
|                                                         |             |          |        |                    |        |         | Figure 9. | Tagged | values | in the SpiderMonkey | JS  | interpreter. |
| struction is                                            | immediately | passed   | to the | first filter       | in the | forward |           |        |        |                     |     |              |
Testingtags,unboxing(extractingtheuntaggedvalue)andboxing
| pipeline. Thus, | forward | filter | optimizations | are | performed | as the |     |     |     |     |     |     |
| --------------- | ------- | ------ | ------------- | --- | --------- | ------ | --- | --- | --- | --- | --- | --- |
(creatingtaggedvalues)aresignificantcosts.Avoidingthesecosts
traceisrecorded.Eachfiltermaypasseachinstructiontothenext
isakeybenefitoftracing.
| filter unchanged, | write | a different | instruction | to  | the next | filter, or |     |     |     |     |     |     |
| ----------------- | ----- | ----------- | ----------- | --- | -------- | ---------- | --- | --- | --- | --- | --- | --- |
writenoinstructionatall.Forexample,theconstantfoldingfilter
| canreplaceamultiplyinstructionlikev |     |     |     | := mul3,1000witha |     |     |     |     |     |     |     |     |
| ----------------------------------- | --- | --- | --- | ----------------- | --- | --- | --- | --- | --- | --- | --- | --- |
13
constantinstructionv =3000. heuristic selects v with minimum v m. The motivation is that this
13
Wecurrentlyapplyfourforwardfilters: freesuparegisterforaslongaspossiblegivenasinglespill.
|     |     |     |     |     |     |     | If we | need to | spill | a value v at this | point, we | generate the |
| --- | --- | --- | --- | --- | --- | --- | ----- | ------- | ----- | ----------------- | --------- | ------------ |
s
On ISAs without floating-point instructions, a soft-float filter restore code just after the code for the current instruction. The
•
convertsfloating-pointLIRinstructionstosequencesofinteger correspondingspillcodeisgeneratedjustafterthelastpointwhere
instructions. v swasused.Theregisterthatwasassignedtov sismarkedfreefor
|     |     |     |     |     |     |     | the preceding | code, | because | that register | can now be | used freely |
| --- | --- | --- | --- | --- | --- | --- | ------------- | ----- | ------- | ------------- | ---------- | ----------- |
CSE(constantsubexpressionelimination),
| •   |     |     |     |     |     |     | withoutaffectingthefollowingcode |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | -------------------------------- | --- | --- | --- | --- | --- |
expressionsimplification,includingconstantfoldingandafew
•
| algebraicidentities(e.g.,a |          |                   | a=0),and |            |                 |     |                   |     |     |     |     |     |
| -------------------------- | -------- | ----------------- | -------- | ---------- | --------------- | --- | ----------------- | --- | --- | --- | --- | --- |
|                            |          |                   | −        |            |                 |     | 6. Implementation |     |     |     |     |     |
| source                     | language | semantic-specific |          | expression | simplification, |     |                   |     |     |     |     |     |
•
primarilyalgebraicidentitiesthatallowDOUBLEtobereplaced To demonstrate the effectiveness of our approach, we have im-
withINT.Forexample,LIRthatconvertsanINTtoaDOUBLE plementedatrace-baseddynamiccompilerfortheSpiderMonkey
andthenbackagainwouldberemovedbythisfilter. JavaScript Virtual Machine (4). SpiderMonkey is the JavaScript
VMembeddedinMozilla’sFirefoxopen-sourcewebbrowser(2),
Whentracerecordingiscompleted,nanojitrunsthebackward whichisusedbymorethan200millionusersworld-wide.Thecore
optimization filters. These are used for optimizations that require ofSpiderMonkeyisabytecodeinterpreterimplementedinC++.
backward program analysis. When running the backward filters, In SpiderMonkey, all JavaScript values are represented by the
nanojitreadsoneLIRinstructionatatime,andthereadsarepassed typejsval.Ajsvalismachinewordinwhichuptothe3ofthe
throughthepipeline. leastsignificantbitsareatypetag,andtheremainingbitsaredata.
Wecurrentlyapplythreebackwardfilters: SeeFigure6fordetails.Allpointerscontainedinjsvalspointto
GC-controlledblocksalignedon8-byteboundaries.
Deaddata-stackstoreelimination.TheLIRtraceencodesmany
| •   |     |     |     |     |     |     | JavaScriptobjectvaluesaremappingsofstring-valuedproperty |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | -------------------------------------------------------- | --- | --- | --- | --- | --- |
storestolocationsintheinterpreterstack.Butthesevaluesare
namestoarbitraryvalues.Theyarerepresentedinoneoftwoways
| never read | back | before exiting | the trace | (by | the interpreter | or  |     |     |     |     |     |     |
| ---------- | ---- | -------------- | --------- | --- | --------------- | --- | --- | --- | --- | --- | --- | --- |
inSpiderMonkey.Mostobjectsarerepresentedbyasharedstruc-
| another | trace). Thus, | stores | to the | stack that | are | overwritten |     |     |     |     |     |     |
| ------- | ------------- | ------ | ------ | ---------- | --- | ----------- | --- | --- | --- | --- | --- | --- |
turaldescription,calledtheobjectshape,thatmapspropertynames
| before | the next exit | are dead. | Stores | to locations |     | that are off |     |     |     |     |     |     |
| ------ | ------------- | --------- | ------ | ------------ | --- | ------------ | --- | --- | --- | --- | --- | --- |
toarrayindexesusingahashtable.Theobjectstoresapointerto
thetopoftheinterpreterstackatfutureexitsarealsodead.
|     |     |     |     |     |     |     | the shape | and the | array | of its own property | values. | Objects with |
| --- | --- | --- | --- | --- | --- | --- | --------- | ------- | ----- | ------------------- | ------- | ------------ |
Deadcall-stackstoreelimination.Thisisthesameoptimization large,uniquesetsofpropertynamesstoretheirpropertiesdirectly
•
| asabove,exceptappliedtotheinterpreter’scallstackusedfor |     |     |     |     |     |     | inahashtable. |     |     |     |     |     |
| ------------------------------------------------------- | --- | --- | --- | --- | --- | --- | ------------- | --- | --- | --- | --- | --- |
functioncallinlining. The garbage collector is an exact, non-generational, stop-the-
worldmark-and-sweepcollector.
| Dead code | elimination. |     | This eliminates | any | operation | that |                                                       |     |     |     |     |     |
| --------- | ------------ | --- | --------------- | --- | --------- | ---- | ----------------------------------------------------- | --- | --- | --- | --- | --- |
| •         |              |     |                 |     |           |      | IntherestofthissectionwediscusskeyareasoftheTraceMon- |     |     |     |     |     |
storestoavaluethatisneverused.
keyimplementation.
| After a | LIR instruction |     | is successfully | read | (“pulled”) | from |     |     |     |     |     |     |
| ------- | --------------- | --- | --------------- | ---- | ---------- | ---- | --- | --- | --- | --- | --- | --- |
thebackwardfilterpipeline,nanojit’scodegeneratoremitsnative 6.1 CallingCompiledTraces
machineinstruction(s)forit.
Compiledtracesarestoredinatracecache,indexedbyintepreter
|     |     |     |     |     |     |     | PC and | type map. | Traces | are compiled | so that they | may be |
| --- | --- | --- | --- | --- | --- | --- | ------ | --------- | ------ | ------------ | ------------ | ------ |
5.2 RegisterAllocation
calledasfunctionsusingstandardnativecallingconventions(e.g.,
| We use a | simple greedy | register | allocator | that | makes | a single | FASTCALLonx86). |     |     |     |     |     |
| -------- | ------------- | -------- | --------- | ---- | ----- | -------- | --------------- | --- | --- | --- | --- | --- |
backward pass over the trace (it is integrated with the code gen- The interpreter must hit a loop edge and enter the monitor in
erator). By the time the allocator has reached an instruction like ordertocallanativetraceforthefirsttime.Themonitorcomputes
v =addv ,v 2,ithasalreadyassignedaregistertov 3.Ifv 1and the current type map, checks the trace cache for a trace for the
| 3   | 1   |     |     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
v 2havenotyetbeenassignedregisters,theallocatorassignsafree currentPCandtypemap,andifitfindsone,executesthetrace.
registertoeach.Iftherearenofreeregisters,avalueisselectedfor To execute a trace, the monitor must build a trace activation
spilling.Weuseaclassheuristicthatselectsthe“oldest”register- record containing imported local and global variables, temporary
carriedvalue(6). stackspace,andspaceforargumentstonativecalls.Thelocaland
TheheuristicconsidersthesetRofvaluesvinregistersimme- globalvaluesarethencopiedfromtheinterpreterstatetothetrace
diatelyafterthecurrentinstructionforspilling.Letv bethelast activationrecord.Then,thetraceiscalledlikeanormalCfunction
m
| instructionbeforethecurrentwhereeachvisreferredto.Thenthe |     |     |     |     |     |     | pointer. |     |     |     |     |     |
| --------------------------------------------------------- | --- | --- | --- | --- | --- | --- | -------- | --- | --- | --- | --- | --- |
472

When a trace call returns, the monitor restores the interpreter Recording is activated by a pointer swap that sets the inter-
state. First, the monitor checks the reason for the trace exit and preter’s dispatch table to call a single “interrupt” routine for ev-
applies blacklisting if needed. Then, it pops or synthesizes inter- ery bytecode. The interrupt routine first calls a bytecode-specific
preterJavaScriptcallstackframesasneeded.Finally,itcopiesthe recording routine. Then, it turns off recording if necessary (e.g.,
importedvariablesbackfromthetraceactivationrecordtothein- thetraceended).Finally,itjumpstothestandardinterpreterbyte-
terpreterstate. codeimplementation.Somebytecodeshaveeffectsonthetypemap
Atleastinthecurrentimplementation,thesestepshaveanon- thatcannotbepredictedbeforeexecutingthebytecode(e.g.,call-
negligibleruntimecost,sominimizingthenumberofinterpreter- ingString.charCodeAt,whichreturnsanintegerorNaN ifthe
to-trace and trace-to-interpreter transitions is essential for perfor- indexargumentisoutofrange).Forthese,wearrangefortheinter-
mance. (see also Section 3.3). Our experiments (see Figure 12) pretertocallintotherecorderagainafterexecutingthebytecode.
show that for programs we can trace well such transitions hap- Sincesuchhooksarerelativelyrare,weembedthemdirectlyinto
peninfrequentlyandhencedonotcontributesignificantlytototal theinterpreter,withanadditionalruntimechecktoseewhethera
runtime. In a few programs, where the system is prevented from recorderiscurrentlyactive.
recording branch traces for hot side exits by aborts, this cost can Whileseparatingtheinterpreterfromtherecorderreducesindi-
risetoupto10%oftotalexecutiontime. vidualcodecomplexity,italsorequirescarefulimplementationand
extensivetestingtoachievesemanticequivalence.
6.2 TraceStitching InsomecasesachievingthisequivalenceisdifficultsinceSpi-
Transitionsfromatracetoabranchtraceatasideexitavoidthe
derMonkeyfollowsafat-bytecodedesign,whichwasfoundtobe
costs of calling traces from the monitor, in a feature called trace beneficialtopureinterpreterperformance.
stitching.Atasideexit,theexitingtraceonlyneedstowritelive In fat-bytecode designs, individual bytecodes can implement
register-carriedvaluesbacktoitstraceactivationrecord.Inourim- complex processing (e.g., the getprop bytecode, which imple-
plementation,identicaltypemapsyieldidenticalactivationrecord mentsfullJavaScriptpropertyvalueaccess,includingspecialcases
layouts, so the trace activation record can be reused immediately forcachedanddensearrayaccess).
bythebranchtrace. Fat bytecodes have two advantages: fewer bytecodes means
In programs with branchy trace trees with small traces, trace lowerdispatchcost,andbiggerbytecodeimplementationsgivethe
stitching has a noticeable cost. Although writing to memory and compilermoreopportunitiestooptimizetheinterpreter.
then soon reading back would be expected to have a high L1 Fat bytecodes are a problem for TraceMonkey because they
cachehitrate,forsmalltracestheincreasedinstructioncounthas require the recorder to reimplement the same special case logic
a noticeable cost. Also, if the writes and reads are very close in the same way. Also, the advantages are reduced because (a)
in the dynamic instruction stream, we have found that current dispatch costs are eliminated entirely in compiled traces, (b) the
x86 processors often incur penalties of 6 cycles or more (e.g., if traces contain only one special case, not the interpreter’s large
theinstructionsusedifferentbaseregisterswithequalvalues,the chunkofcode,and(c)TraceMonkeyspendslesstimerunningthe
processormaynotbeabletodetectthattheaddressesarethesame baseinterpreter.
rightaway). Onewaywehavemitigatedtheseproblemsisbyimplementing
Thealternatesolutionistorecompileanentiretracetree,thus certaincomplexbytecodesintherecorderassequencesofsimple
achieving inter-trace register allocation (10). The disadvantage is bytecodes.Expressingtheoriginalsemanticsthiswayisnottoodif-
thattreerecompilationtakestimequadraticinthenumberoftraces. ficult,andrecordingsimplebytecodesismucheasier.Thisenables
We believe that the cost of recompiling a trace tree every time ustoretaintheadvantagesoffatbytecodeswhileavoidingsomeof
a branch is added would be prohibitive. That problem might be theirproblemsfortracerecording.Thisisparticularlyeffectivefor
mitigated by recompiling only at certain points, or only for very fatbytecodesthatrecursebackintotheinterpreter,forexampleto
hot,stabletrees. convertanobjectintoaprimitivevaluebyinvokingawell-known
In the future, multicore hardware is expected to be common, methodontheobject,sinceitletsusinlinethisfunctioncall.
making background tree recompilation attractive. In a closely re- Itisimportanttonotethatwesplitfatopcodesintothinnerop-
lated project (13) background recompilation yielded speedups of codesonlyduringrecording.Whenrunningpurelyinterpretatively
up to 1.25x on benchmarks with many branch traces. We plan to (i.e.codethathasbeenblacklisted),theinterpreterdirectlyandef-
applythistechniquetoTraceMonkeyasfuturework. ficientlyexecutesthefatopcodes.
6.3 TraceRecording
6.4 Preemption
ThejobofthetracerecorderistoemitLIRwithidenticalsemantics
tothecurrentlyrunninginterpreterbytecodetrace.Agoodimple- SpiderMonkey,likemanyVMs,needstopreempttheuserprogram
mentationshouldhavelowimpactonnon-tracinginterpreterper- periodically. The main reasons are to prevent infinitely looping
formanceandaconvenientwayforimplementerstomaintainse- scriptsfromlockingupthehostsystemandtoscheduleGC.
manticequivalence. Intheinterpreter,thishadbeenimplementedbysettinga“pre-
Inourimplementation,theonlydirectmodificationtotheinter- empt now” flag that was checked on every backward jump. This
preterisacalltothetracemonitoratloopedges.Inourbenchmark strategycarriedoverintoTraceMonkey:theVMinsertsaguardon
results(seeFigure12)thetotaltimespentinthemonitor(forall the preemption flag at every loop edge. We measured less than a
activities) is usually less than 5%, so we consider the interpreter 1%increaseinruntimeonmostbenchmarksforthisextraguard.
impact requirement met. Incrementing the loop hit counter is ex- Inpractice,thecostisdetectableonlyforprogramswithveryshort
pensivebecauseitrequiresustolookuptheloopinthetracecache, loops.
butwehavetunedourloopstobecomehotandtraceveryquickly We tested and rejected a solution that avoided the guards by
(ontheseconditeration).Thehitcounterimplementationcouldbe compiling the loop edge as an unconditional jump, and patching
improved,whichmightgiveusasmallincreaseinoverallperfor- the jump target to an exit routine when preemption is required.
mance,aswellasmoreflexibilitywithtuninghotnessthresholds. This solution can make the normal case slightly faster, but then
Oncealoopisblacklistedwenevercallintothetracemonitorfor preemptionbecomesveryslow.Theimplementationwasalsovery
thatloop(seeSection3.3). complex,especiallytryingtorestartexecutionafterthepreemption.
473

6.5 CallingExternalFunctions
?>9@AJ.D<F@-<>2.@A:0>#3$4,56#
| Likemostinterpreters,SpiderMonkeyhasaforeignfunctioninter- |     |     |     |     |     |     |     | ?>9@AJ.0A:</C./8-2#3$4%56# |     |     |     |     |     |     |     |
| ---------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | -------------------------- | --- | --- | --- | --- | --- | --- | --- |
| face(FFI)thatallowsittocallCbuiltinsandhostsystemfunctions |     |     |     |     |     |     |     | ?>9@AJ.><J/F80-#3$4$56#    |     |     |     |     |     |     |     |
?>9@AJ.B<?><#3$4(56#
| (e.g.,webbrowsercontrolandDOMaccess).TheFFIhasastan- |     |     |     |     |     |     |     | ?>9@AJ.1<?2)'#3%4(56# |     |     |     |     |     |     |     |
| ---------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --------------------- | --- | --- | --- | --- | --- | --- | --- |
92J25:.-A<#3'4%56#
dardsignatureforJS-callablefunctions,thekeyargumentofwhich
7<>;.?:2/>9<F.A897#3*4$56#
| isanarrayofboxedvalues.Externalfunctionscalledthroughthe |     |     |     |     |     |     |     | 7<>;.:<9I<F.?07?#3(4,56# |     |     |     |     |     |     |     |
| -------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | --- | --- | --- | --- | --- | --- |
7<>;./89-@/#3'4,56#
FFIinteractwiththeprogramstatethroughaninterpreterAPI(e.g.,
-<>2.B897<>.5:<91#3$4!56#
| toreadapropertyfromanargument).Therearealsocertaininter- |     |     |     |     |     |     |     | -<>2.B897<>.>8H2#3$4$56# |     |     |     |     |     |     |     |
| -------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | --- | --- | --- | --- | --- | --- |
/9=:>8.?;<$#3(4,56#
preterbuiltinsthatdonotusetheFFI,butinteractwiththeprogram
/9=:>8.7-(#3%4&56#
| state in the | same way, | such as | the CallIteratorNext |     |     | function |     | /9=:>8.<2?#3$4)56# |     |     |     |     |     |     |     |
| ------------ | --------- | ------- | -------------------- | --- | --- | -------- | --- | ------------------ | --- | --- | --- | --- | --- | --- | --- |
/8A>98FG8E.92/09?@D2#3$4!56#
usedwithiteratorobjects.TraceMonkeymustsupportthisFFIin
1@>8:?.A?@2D2.1@>?#3%4*56#
| ordertospeedupcodethatinteractswiththehostsysteminsidehot |     |     |     |     |     |     |     | 1@>8:?.1@>E@?2.<A-#3%(4%56# |     |     |     |     |     |     |     |
| --------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --------------------------- | --- | --- | --- | --- | --- | --- | --- |
1@>8:?.1@>?.@A.1=>2#3+4*56#
loops.
1@>8:?.&1@>.1@>?.@A.1=>2#3%(4(56#
| CallingexternalfunctionsfromTraceMonkeyispotentiallydif- |     |     |     |     |     |     |     | <//2??.A?@2D2#3&4!56# |     |     |     |     |     |     |     |
| -------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --------------------- | --- | --- | --- | --- | --- | --- | --- |
<//2??.A18-=#3'4%56#
ficultbecausetracesdonotupdatetheinterpreterstateuntilexit-
<//2??.B<AAC0/;#3%4%56#
| ing.Inparticular,externalfunctionsmayneedthecallstackorthe |     |     |     |     |     |     |     | <//2??.1@A<9=.>922?#3!4,56# |     |     |     |     |     |     |     |
| ---------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --------------------------- | --- | --- | --- | --- | --- | --- | --- |
&-.9<=>9</2#3$4%56#
globalvariables,buttheymaybeoutofdate.
&-.789:;#3%4,56#
| For the | out-of-date | call stack | problem, | we  | refactored | some of |     | &-./012#3%4%56# |     |     |     |     |     |     |     |
| ------- | ----------- | ---------- | -------- | --- | ---------- | ------- | --- | --------------- | --- | --- | --- | --- | --- | --- | --- |
theinterpreterAPIimplementationfunctionstore-materializethe !"# $!"# %!"# &!"# '!"# (!"# )!"# *!"# +!"# ,!"# $!!"#
interpretercallstackondemand.
WedevelopedaC++staticanalysisandannotatedsomeinter- KA>29:92># L<ID2#
| preter functions | in order | to verify | that | the call | stack | is refreshed |     |     |     |     |     |     |     |     |     |
| ---------------- | -------- | --------- | ---- | -------- | ----- | ------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- |
atanypointitneedstobeused.Inordertoaccessthecallstack,
|            |         |           |           |             |     |        | Figure | 11. | Fraction | of dynamic |     | bytecodes | executed |     | by inter- |
| ---------- | ------- | --------- | --------- | ----------- | --- | ------ | ------ | --- | -------- | ---------- | --- | --------- | -------- | --- | --------- |
| a function | must be | annotated | as either | FORCESSTACK |     | or RE- |        |     |          |            |     |           |          |     |           |
preterandonnativetraces.Thespeedupvs.interpreterisshown
QUIRESSTACK.Theseannotationsarealsorequiredinordertocall
|     |     |     |     |     |     |     | in  | parentheses | next | to each | test. The | fraction | of  | bytecodes | exe- |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------- | ---- | ------- | --------- | -------- | --- | --------- | ---- |
REQUIRESSTACKfunctions,whicharepresumedtoaccessthecall
|                     |             |     |      |         |             |         | cuted | while | recording | is too | small | to see | in this | figure, | except |
| ------------------- | ----------- | --- | ---- | ------- | ----------- | ------- | ----- | ----- | --------- | ------ | ----- | ------ | ------- | ------- | ------ |
| stack transitively. | FORCESSTACK |     | is a | trusted | annotation, | applied |       |       |           |        |       |        |         |         |        |
forcrypto-md5,wherefully3%ofbytecodesareexecutedwhile
toonly5functions,thatmeansthefunctionrefreshesthecallstack.
|     |     |     |     |     |     |     | recording. |     | In most | of the tests, | almost | all | the bytecodes |     | are exe- |
| --- | --- | --- | --- | --- | --- | --- | ---------- | --- | ------- | ------------- | ------ | --- | ------------- | --- | -------- |
REQUIRESSTACKisanuntrustedannotationthatmeansthefunc-
cutedbycompiledtraces.Threeofthebenchmarksarenottraced
tionmayonlybecalledifthecallstackhasalreadybeenrefreshed.
atallandrunintheinterpreter.
| Similarly, | we detect | when | host functions |     | attempt | to directly |     |     |     |     |     |     |     |     |     |
| ---------- | --------- | ---- | -------------- | --- | ------- | ----------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
readorwriteglobalvariables,andforcethecurrentlyrunningtrace
| to side exit. | This is | necessary | since we | cache | and unbox | global |     |     |     |     |     |     |     |     |     |
| ------------- | ------- | --------- | -------- | ----- | --------- | ------ | --- | --- | --- | --- | --- | --- | --- | --- | --- |
loopsandheavilybranchingcode,andaspecializedfuzztesterin-
variablesintotheactivationrecordduringtraceexecution.
deedrevealedseveralregressionswhichwesubsequentlycorrected.
| Since both | call-stack | access | and | global | variable | access are |     |     |     |     |     |     |     |     |     |
| ---------- | ---------- | ------ | --- | ------ | -------- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
rarelyperformedbyhostfunctions,performanceisnotsignificantly
| affectedbythesesafetymechanisms. |     |     |     |     |     |     | 7.  | Evaluation |     |     |     |     |     |     |     |
| -------------------------------- | --- | --- | --- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
Anotherproblemisthatexternalfunctionscanreentertheinter-
|     |     |     |     |     |     |     | We  | evaluated | our | JavaScript | tracing | implementation |     | using | Sun- |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | ---------- | ------- | -------------- | --- | ----- | ---- |
preterbycallingscripts,whichinturnagainmightwanttoaccess
Spider,theindustrystandardJavaScriptbenchmarksuite.SunSpi-
thecallstackorglobalvariables.Toaddressthisproblem,wemade
derconsistsof26short-running(lessthan250ms,average26ms)
theVMsetaflagwhenevertheinterpreterisreenteredwhileacom-
JavaScriptprograms.Thisisinstarkcontrasttobenchmarksuites
piledtraceisrunning.
suchasSpecJVM98(3)usedtoevaluatedesktopandserverJava
Everycalltoanexternalfunctionthenchecksthisflagandexits
VMs.Manyprogramsinthosebenchmarksuselargedatasetsand
thetraceimmediatelyafterreturningfromtheexternalfunctioncall
executeforminutes.TheSunSpiderprogramscarryoutavarietyof
ifitisset.Therearemanyexternalfunctionsthatseldomornever
tasks,primarily3drendering,bit-bashing,cryptographicencoding,
| reenter, and | they can | be called | without | problem, | and | will cause |     |     |     |     |     |     |     |     |     |
| ------------ | -------- | --------- | ------- | -------- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
mathkernels,andstringprocessing.
traceexitonlyifnecessary.
|           |       |             |             |     |       |             |     | All experiments |     | were performed |     | on a | MacBook | Pro | with 2.2 |
| --------- | ----- | ----------- | ----------- | --- | ----- | ----------- | --- | --------------- | --- | -------------- | --- | ---- | ------- | --- | -------- |
| The FFI’s | boxed | value array | requirement |     | has a | performance |     |                 |     |                |     |      |         |     |          |
GHzCore2processorand2GBRAMrunningMacOS10.5.
| cost, so we | defined a | new FFI | that allows | C   | functions | to be an- |     |           |          |     |      |          |            |     |          |
| ----------- | --------- | ------- | ----------- | --- | --------- | --------- | --- | --------- | -------- | --- | ---- | -------- | ---------- | --- | -------- |
|             |           |         |             |     |           |           |     | Benchmark | results. | The | main | question | is whether |     | programs |
notatedwiththeirargumenttypessothatthetracercancallthem
runfasterwithtracing.Forthis,weranthestandardSunSpidertest
directly,withoutunnecessaryargumentconversions.
|     |     |     |     |     |     |     | driver, | which | starts | a JavaScript | interpreter, |     | loads | and runs | each |
| --- | --- | --- | --- | --- | --- | --- | ------- | ----- | ------ | ------------ | ------------ | --- | ----- | -------- | ---- |
Currently,wedonotsupportcallingnativepropertygetandset
|     |     |     |     |     |     |     | program | once | for | warmup, | then loads | and | runs each | program | 10  |
| --- | --- | --- | --- | --- | --- | --- | ------- | ---- | --- | ------- | ---------- | --- | --------- | ------- | --- |
overridefunctionsorDOMfunctionsdirectlyfromtrace.Support
timesandreportstheaveragetimetakenbyeach.Weran4differ-
isplannedfuturework.
entconfigurationsforcomparison:(a)SpiderMonkey,thebaseline
interpreter,(b)TraceMonkey,(d)SquirrelFishExtreme(SFX),the
6.6 Correctness call-threaded JavaScript interpreter used in Apple’s WebKit, and
During development, we had access to existing JavaScript test (e)V8,themethod-compilingJavaScriptVMfromGoogle.
suites, but most of them were not designed with tracing VMs in Figure10showstherelativespeedupsachievedbytracing,SFX,
mindandcontainedfewloops. andV8againstthebaseline(SpiderMonkey).Tracingachievesthe
One tool that helped us greatly was Mozilla’s JavaScript fuzz bestspeedupsininteger-heavybenchmarks,uptothe25xspeedup
tester, JSFUNFUZZ, which generates random JavaScript programs onbitops-bitwise-and.
by nesting random language elements. We modified TraceMonkey is the fastest VM on 9 of the 26 benchmarks
JSFUNFUZZ
togenerateloops,andalsototestmoreheavilycertainconstructs (3d-morph, bitops-3bit-bits-in-byte, bitops-bitwise-
wesuspectedwouldrevealflawsinourimplementation.Forexam- and,crypto-sha1,math-cordic,math-partial-sums,math-
ple,wesuspectedbugsinTraceMonkey’shandlingoftype-unstable spectral-norm, string-base64, string-validate-input).
474

%#"
&'()*+,"
| %!" |     |     |     |     |     |     | -./" |     |
| --- | --- | --- | --- | --- | --- | --- | ---- | --- |
01"
$#"
$!"
#"
!"
Figure10. Speedupvs.abaselineJavaScriptinterpreter(SpiderMonkey)forourtrace-basedJITcompiler,Apple’sSquirrelFishExtreme
inlinethreadinginterpreterandGoogle’sV8JScompiler.Oursystemgeneratesparticularlyefficientcodeforprogramsthatbenefitmostfrom
typespecialization,whichincludesSunSpiderBenchmarkprogramsthatperformbitmanipulation.Wetype-specializethecodeinquestion
touseintegerarithmetic,whichsubstantiallyimprovesperformance.Foroneofthebenchmarkprogramsweexecute25timesfasterthan
theSpiderMonkeyinterpreter,andalmost5timesfasterthanV8andSFX.ForalargenumberofbenchmarksallthreeVMsproducesimilar
results.Weperformworstonbenchmarkprogramsthatwedonottraceandinsteadfallbackontotheinterpreter.Thisincludestherecursive
benchmarksaccess-binary-treesandcontrol-flow-recursive,forwhichwecurrentlydon’tgenerateanynativecode.
Inparticular,thebitopsbenchmarksareshortprogramsthatper- Two programs trace well, but have a long compilation time.
•
formmanybitwiseoperations,soTraceMonkeycancovertheen- access-nbodyformsalargenumberoftraces(81).crypto-md5
tireprogramwith1or2tracesthatoperateonintegers.TraceMon- formsoneverylongtrace.Weexpecttoimproveperformance
keyrunsalltheotherprogramsinthissetalmostentirelyasnative onthisprogramsbyimprovingthecompilationspeedofnano-
| code.                                                  |              |            |            |           | jit.             |             |                       |                |
| ------------------------------------------------------ | ------------ | ---------- | ---------- | --------- | ---------------- | ----------- | --------------------- | -------------- |
| regexp-dna                                             | is dominated | by regular | expression | matching, |                  |             |                       |                |
|                                                        |              |            |            |           | Some programs    | trace very  | well, and speed       | up compared to |
| whichisimplementedinall3VMsbyaspecialregularexpression |              |            |            |           | •                |             |                       |                |
|                                                        |              |            |            |           | the interpreter, | but are not | as fast as SFX and/or | V8, namely     |
compiler.Thus,performanceonthisbenchmarkhaslittlerelation bitops-bits-in-byte, bitops-nsieve-bits, access-
tothetracecompilationapproachdiscussedinthispaper. fannkuch,access-nsieve,andcrypto-aes.Thereasonis
TraceMonkey’ssmallerspeedupsontheotherbenchmarkscan
|     |     |     |     |     | not clear, | but all of these | programs have nested | loops with |
| --- | --- | --- | --- | --- | ---------- | ---------------- | -------------------- | ---------- |
beattributedtoafewspecificcauses:
smallbodies,sowesuspectthattheimplementationhasarela-
tivelyhighcostforcallingnestedtraces.string-fastatraces
well,butitsruntimeisdominatedbystringprocessingbuiltins,
| The implementation | does     | not currently   | trace | recursion, so |                                                       |     |     |     |
| ------------------ | -------- | --------------- | ----- | ------------- | ----------------------------------------------------- | --- | --- | --- |
| •                  |          |                 |       |               | whichareunaffectedbytracingandseemtobelessefficientin |     |     |     |
| TraceMonkey        | achieves | a small speedup | or no | speedup on    |                                                       |     |     |     |
SpiderMonkeythaninthetwootherVMs.
| benchmarks | that use | recursion extensively: |     | 3d-cube, 3d- |     |     |     |     |
| ---------- | -------- | ---------------------- | --- | ------------ | --- | --- | --- | --- |
raytrace,access-binary-trees,string-tagcloud,and
controlflow-recursive. Detailedperformancemetrics.InFigure11weshowthefrac-
tionofinstructionsinterpretedandthefractionofinstructionsexe-
The implementation does not currently trace eval and some cutedasnativecode.Thisfigureshowsthatformanyprograms,we
•
other functions implemented in C. Because date-format- areabletoexecutealmostallthecodenatively.
tofte and date-format-xparb use such functions in their Figure12breaksdownthetotalexecutiontimeintofouractiv-
mainloops,wedonottracethem. ities:interpretingbytecodeswhilenotrecording,recordingtraces
The implementation does not currently trace through regular (including time taken to interpret the recorded trace), compiling
•
expression replace operations. The replace function can be tracestonativecode,andexecutingnativecodetraces.
passedafunctionobjectusedtocomputethereplacementtext. These detailed metrics allow us to estimate parameters for a
Our implementation currently does not trace functions called simple model of tracing performance. These estimates should be
asreplacefunctions.Theruntimeofstring-unpack-codeis considered very rough, as the values observed on the individual
dominatedbysuchareplacecall. benchmarks have large standard deviations (on the order of the
475

Loops Trees Traces Aborts Flushes Trees/Loop Traces/Tree Traces/Loop Speedup
| 3d-cube                  |     |     | 25 27 |     | 29 3   | 0   | 1.1 |     | 1.1 |      | 1.2 2.20x  |
| ------------------------ | --- | --- | ----- | --- | ------ | --- | --- | --- | --- | ---- | ---------- |
| 3d-morph                 |     |     | 5     | 8   | 8 2    | 0   | 1.6 |     | 1.0 |      | 1.6 2.86x  |
| 3d-raytrace              |     |     | 10 25 |     | 100 10 | 1   | 2.5 |     | 4.0 | 10.0 | 1.18x      |
| access-binary-trees      |     |     | 0     | 0   | 0 5    | 0   | -   |     | -   |      | - 0.93x    |
| access-fannkuch          |     |     | 10 34 |     | 57 24  | 0   | 3.4 |     | 1.7 |      | 5.7 2.20x  |
| access-nbody             |     |     | 8 16  |     | 18 5   | 0   | 2.0 |     | 1.1 |      | 2.3 4.19x  |
| access-nsieve            |     |     | 3     | 6   | 8 3    | 0   | 2.0 |     | 1.3 |      | 2.7 3.05x  |
| bitops-3bit-bits-in-byte |     |     | 2     | 2   | 2 0    | 0   | 1.0 |     | 1.0 |      | 1.0 25.47x |
| bitops-bits-in-byte      |     |     | 3     | 3   | 4 1    | 0   | 1.0 |     | 1.3 |      | 1.3 8.67x  |
| bitops-bitwise-and       |     |     | 1     | 1   | 1 0    | 0   | 1.0 |     | 1.0 |      | 1.0 25.20x |
| bitops-nsieve-bits       |     |     | 3     | 3   | 5 0    | 0   | 1.0 |     | 1.7 |      | 1.7 2.75x  |
| controlflow-recursive    |     |     | 0     | 0   | 0 1    | 0   | -   |     | -   |      | - 0.98x    |
| crypto-aes               |     |     | 50 72 |     | 78 19  | 0   | 1.4 |     | 1.1 |      | 1.6 1.64x  |
| crypto-md5               |     |     | 4     | 4   | 5 0    | 0   | 1.0 |     | 1.3 |      | 1.3 2.30x  |
| crypto-sha1              |     |     | 5     | 5   | 10 0   | 0   | 1.0 |     | 2.0 |      | 2.0 5.95x  |
| date-format-tofte        |     |     | 3     | 3   | 4 7    | 0   | 1.0 |     | 1.3 |      | 1.3 1.07x  |
| date-format-xparb        |     |     | 3     | 3   | 11 3   | 0   | 1.0 |     | 3.7 |      | 3.7 0.98x  |
| math-cordic              |     |     | 2     | 4   | 5 1    | 0   | 2.0 |     | 1.3 |      | 2.5 4.92x  |
| math-partial-sums        |     |     | 2     | 4   | 4 1    | 0   | 2.0 |     | 1.0 |      | 2.0 5.90x  |
| math-spectral-norm       |     |     | 15 20 |     | 20 0   | 0   | 1.3 |     | 1.0 |      | 1.3 7.12x  |
| regexp-dna               |     |     | 2     | 2   | 2 0    | 0   | 1.0 |     | 1.0 |      | 1.0 4.21x  |
| string-base64            |     |     | 3     | 5   | 7 0    | 0   | 1.7 |     | 1.4 |      | 2.3 2.53x  |
| string-fasta             |     |     | 5 11  |     | 15 6   | 0   | 2.2 |     | 1.4 |      | 3.0 1.49x  |
| string-tagcloud          |     |     | 3     | 6   | 6 5    | 0   | 2.0 |     | 1.0 |      | 2.0 1.09x  |
| string-unpack-code       |     |     | 4     | 4   | 37 0   | 0   | 1.0 |     | 9.3 |      | 9.3 1.20x  |
| string-validate-input    |     |     | 6 10  |     | 13 1   | 0   | 1.7 |     | 1.3 |      | 2.2 1.86x  |
Figure13. DetailedtracerecordingstatisticsfortheSunSpiderbenchmarkset.
mean).Weexcluderegexp-dnafromthefollowingcalculations, fastestavailableJavaScriptinlinethreadedinterpreter(SFX)on9
| becausemostofitstimeisspentintheregularexpressionmatcher, |                |             |                 |              |          | of26benchmarks. |     |     |     |     |     |
| --------------------------------------------------------- | -------------- | ----------- | --------------- | ------------ | -------- | --------------- | --- | --- | --- | --- | --- |
| which has                                                 | much different | performance | characteristics |              | from the |                 |     |     |     |     |     |
| other programs.                                           | (Note that     | this only   | makes           | a difference | of about |                 |     |     |     |     |     |
10%intheresults.)Dividingthetotalexecutiontimeinprocessor
clock cycles by the number of bytecodes executed in the base 8. RelatedWork
| interpreter | shows that on average, |       | a bytecode | executes      | in about |                    |       |             |                    |     |                     |
| ----------- | ---------------------- | ----- | ---------- | ------------- | -------- | ------------------ | ----- | ----------- | ------------------ | --- | ------------------- |
|             |                        |       |            |               |          | Trace optimization |       | for dynamic | languages.         |     | The closest area of |
| 35 cycles.  | Native traces take     | about | 9 cycles   | per bytecode, | a 3.9x   |                    |       |             |                    |     |                     |
|             |                        |       |            |               |          | related work       | is on | applying    | trace optimization |     | to type-specialize  |
speedupovertheinterpreter.
|     |     |     |     |     |     | dynamic languages. |     | Existing | work shares | the | idea of generating |
| --- | --- | --- | --- | --- | --- | ------------------ | --- | -------- | ----------- | --- | ------------------ |
Usingsimilarcomputations,wefindthattracerecordingtakes
|     |     |     |     |     |     | type-specialized | code | speculatively | with | guards | along interpreter |
| --- | --- | --- | --- | --- | --- | ---------------- | ---- | ------------- | ---- | ------ | ----------------- |
about3800cyclesperbytecode,andcompilation3150cyclesper
traces.
bytecode.Hence,duringrecordingandcompilingtheVMrunsat
|     |     |     |     |     |     | To our | knowledge, | Rigo’s | Psyco | (16) is | the only published |
| --- | --- | --- | --- | --- | --- | ------ | ---------- | ------ | ----- | ------- | ------------------ |
1/200thespeedoftheinterpreter.Becauseitcosts6950cyclesto
type-specializingtracecompilerforadynamiclanguage(Python).
compileabytecode,andwesave26cycleseachtimethatcodeis
Psycodoesnotattempttoidentifyhotloopsorinlinefunctioncalls.
runnatively,webreakevenafterrunningatrace270times.
Instead,Psycotransformsloopstomutualrecursionbeforerunning
TheotherVMswecomparedwithachieveanoverallspeedup
andtracesalloperations.
| of 3.0x relative | to our baseline | interpreter. |     | Our estimated | native |     |     |     |     |     |     |
| ---------------- | --------------- | ------------ | --- | ------------- | ------ | --- | --- | --- | --- | --- | --- |
Pall’sLuaJITisaLuaVMindevelopmentthatusestracecom-
| code speedup | of 3.9x is significantly |     | better. | This | suggests that |     |     |     |     |     |     |
| ------------ | ------------------------ | --- | ------- | ---- | ------------- | --- | --- | --- | --- | --- | --- |
pilationideas.(1).TherearenopublicationsonLuaJITbutthecre-
ourcompilationtechniquescangeneratemoreefficientnativecode
atorhastoldusthatLuaJIThasasimilardesigntooursystem,but
thananyothercurrentJavaScriptVM.
|     |     |     |     |     |     | will use a | lessaggressive | type | speculation | (e.g., | using a floating- |
| --- | --- | --- | --- | --- | --- | ---------- | -------------- | ---- | ----------- | ------ | ----------------- |
Theseestimatesalsoindicatethatourstartupperformancecould
pointrepresentationforallnumbervalues)anddoesnotgenerate
besubstantiallybetterifweimprovedthespeedoftracerecording
nestedtracesfornestedloops.
andcompilation.Theestimated200xslowdownforrecordingand
|     |     |     |     |     |     | General | trace | optimization. | General | trace | optimization has |
| --- | --- | --- | --- | --- | --- | ------- | ----- | ------------- | ------- | ----- | ---------------- |
compilationisveryrough,andmaybeinfluencedbystartupfactors
|     |     |     |     |     |     | a longer history | that | has treated | mostly | native | code and typed |
| --- | --- | --- | --- | --- | --- | ---------------- | ---- | ----------- | ------ | ------ | -------------- |
intheinterpreter(e.g.,cachesthathavenotwarmedupyetduring languageslikeJava.Thus,thesesystemshavefocusedlessontype
recording). One observation supporting this conjecture is that in specializationandmoreonotheroptimizations.
thetracer,interpretedbytecodestakeabout180cyclestorun.Still, Dynamo(7)byBalaetal,introducednativecodetracingasa
recordingandcompilationareclearlybothexpensive,andabetter replacementforprofile-guidedoptimization(PGO).Amajorgoal
implementation, possibly including redesign of the LIR abstract was to perform PGO online so that the profile was specific to
syntaxorencoding,wouldimprovestartupperformance. thecurrentexecution.Dynamousedloopheadersascandidatehot
Ourperformanceresultsconfirmthattypespecializationusing traces,butdidnottrytocreatelooptracesspecifically.
trace trees substantially improves performance. We are able to Trace trees were originally proposed by Gal et al. (11) in the
outperformthefastestavailableJavaScriptcompiler(V8)andthe context of Java, a statically typed language. Their trace trees ac-
tuallyinlinedpartsofouterloopswithintheinnerloops(because
476

eratenativecodewithnearlythesamestructurebutbetterperfor-
=<6>?J+B:F>*:</+>?7-<#0(1923# mance.
=<6>?J+-?7:,A+,5*/#0(1$23#
=<6>?J+<:J,F5-*#0(1(23# Call threading, also known as context threading (8), compiles
=<6>?J+@:=<:#0(1C23#
methods by generating a native call instruction to an interpreter
=<6>?J+.:=/&%#0$1C23#
6/J/27+*?:#0%1$23# methodforeachinterpreterbytecode.Acall-returnpairhasbeen
4:<8+=7/,<6:F+?564#0D1(23#
showntobeapotentiallymuchmoreefficientdispatchmechanism
4:<8+7:6I:F+=-4=#0C1923#
4:<8+,56*>,#0%1923# thantheindirectjumpsusedinstandardbytecodeinterpreters.
*:</+@564:<+27:6.#0(1!23#
*:</+@564:<+<5H/#0(1(23# Inline threading (15) copies chunks of interpreter native code
,6;7<5+=8:(#0C1923# whichimplementtherequiredbytecodesintoanativecodecache,
,6;7<5+4*C#0$1)23#
,6;7<5+:/=#0(1&23# thusactingasasimpleper-methodJITcompilerthateliminatesthe
,5?<65FG5E+6/,-6=>B/#0(1!23# dispatchoverhead.
.><57=+?=>/B/+.><=#0$1D23#
.><57=+.><E>=/+:?*#0$C1$23# Neithercallthreadingnorinlinethreadingperformtypespecial-
.><57=+.><=+>?+.;</#0'1D23# ization.
.><57=+).><+.><=+>?+.;</#0$C1C23#
:,,/==+?=>/B/#0)1!23# Apple’s SquirrelFish Extreme (5) is a JavaScript implementa-
:,,/==+?.5*;#0%1$23# tionbasedoncallthreadingwithselectiveinlinethreading.Com-
:,,/==+@:??A-,8#0$1$23#
:,,/==+.>?:6;+<6//=#0!1923# bined with efficient interpreter engineering, these threading tech-
)*+6:;<6:,/#0(1$23# niqueshavegivenSFXexcellentperformanceonthestandardSun-
)*+45678#0$1923#
)*+,-./#0$1$23# Spiderbenchmarks.
!"# $!"# %!"# &!"# '!"# (!!"# Google’s V8 is a JavaScript implementation primarily based
on inline threading, with call threading only for very complex
K?</676/<# L5?><56# M/,56*# N547>F/# N:FF#O6:,/# M-?#O6:,/# operations.
9. Conclusions
Figure12. FractionoftimespentonmajorVMactivities.The
Thispaperdescribedhowtorundynamiclanguagesefficientlyby
speedup vs. interpreter is shown in parentheses next to each test.
recording hot traces and generating type-specialized native code.
MostprogramswheretheVMspendsthemajorityofitstimerun-
Ourtechniquefocusesonaggressivelyinlinedloops,andforeach
ningnativecodehaveagoodspeedup.Recordingandcompilation
loop, it generates a tree of native code traces representing the
costscanbesubstantial;speedingupthosepartsoftheimplemen-
paths and value types through the loop observed at run time. We
tationwouldimproveSunSpiderperformance.
explainedhowtoidentifyloopnestingrelationshipsandgenerate
nested traces in order to avoid excessive code duplication due
to the many paths through a loop nest. We described our type
specialization algorithm. We also described our trace compiler,
innerloopsbecomehotfirst),leadingtomuchgreatertailduplica- which translates a trace from an intermediate representation to
tion. optimizednativecodeintwolinearpasses.
YETI, from Zaleski et al. (19) applied Dynamo-style tracing Our experimental results show that in practice loops typically
to Java in order to achieve inlining, indirect jump elimination, areenteredwithonlyafewdifferentcombinationsofvaluetypes
andotheroptimizations.Theirprimaryfocuswasondesigningan ofvariables.Thus,asmallnumberoftracesperloopissufficient
interpreterthatcouldeasilybegraduallyre-engineeredasatracing to run a program efficiently. Our experiments also show that on
VM. programsamenabletotracing,weachievespeedupsof2xto20x.
Suganumaetal.(18)describedregion-basedcompilation(RBC),
arelativeoftracing.Aregionisansubprogramworthoptimizing 10. FutureWork
thatcanincludesubsetsofanynumberofmethods.Thus,thecom-
Work is underway in a number of areas to further improve the
pilerhasmoreflexibilityandcanpotentiallygeneratebettercode,
performanceofourtrace-basedJavaScriptcompiler.Wecurrently
buttheprofilingandcompilationsystemsarecorrespondinglymore
do not trace across recursive function calls, but plan to add the
complex.
supportforthiscapabilityinthenearterm.Wearealsoexploring
Type specialization for dynamic languages. Dynamic lan-
adoptionoftheexistingworkontreerecompilationinthecontext
guageimplementorshavelongrecognizedtheimportanceoftype
ofthepresenteddynamiccompilerinordertominimizeJITpause
specializationforperformance.Mostpreviousworkhasfocusedon
timesandobtainthebestofbothworlds,fasttreestitchingaswell
methodsinsteadoftraces.
astheimprovedcodequalityduetotreerecompilation.
Chambers et. al (9) pioneered the idea of compiling multiple
We also plan on adding support for tracing across regular ex-
versionsofaprocedurespecializedfortheinputtypesinthelan-
pression substitutions using lambda functions, function applica-
guage Self. In one implementation, they generated a specialized
tions and expression evaluation using eval. All these language
methodonlineeachtimeamethodwascalledwithnewinputtypes.
constructs are currently executed via interpretation, which limits
In another, they used an offline whole-program static analysis to
ourperformanceforapplicationsthatusethosefeatures.
inferinputtypesandconstantreceivertypesatcallsites.Interest-
ingly,thetwotechniquesproducednearlythesameperformance.
Acknowledgments
Salib(17)designedatypeinferencealgorithmforPythonbased
ontheCartesianProductAlgorithmandusedtheresultstospecial- Parts of this effort have been sponsored by the National Science
izeontypesandtranslatetheprogramtoC++. FoundationundergrantsCNS-0615443andCNS-0627747,aswell
McCloskey (14) has work in progress based on a language- asbytheCaliforniaMICROProgramandindustrialsponsorSun
independent type inference that is used to generate efficient C MicrosystemsunderProjectNo.07-127.
implementationsofJavaScriptandPythonprograms. TheU.S.Governmentisauthorizedtoreproduceanddistribute
Nativecodegenerationbyinterpreters.Thetraditionalinter- reprintsforGovernmentalpurposesnotwithstandinganycopyright
preter design is a virtual machine that directly executes ASTs or annotationthereon.Anyopinions,findings,andconclusionsorrec-
machine-code-likebytecodes.Researchershaveshownhowtogen- ommendations expressed here are those of the author and should
477

not be interpreted as necessarily representing the official views, [10] A.Gal. EfficientBytecodeVerificationandCompilationinaVirtual
policies or endorsements, either expressed or implied, of the Na- MachineDissertation. PhDthesis,UniversityOfCalifornia,Irvine,
2006.
tionalSciencefoundation(NSF),anyotheragencyoftheU.S.Gov-
ernment,oranyofthecompaniesmentionedabove. [11] A.Gal,C.W.Probst,andM.Franz. HotpathVM:AneffectiveJIT
|     |     |     |     |     |     |     | compiler      | for resource-constrained |     | devices.          | In Proceedings | of the |
| --- | --- | --- | --- | --- | --- | --- | ------------- | ------------------------ | --- | ----------------- | -------------- | ------ |
|     |     |     |     |     |     |     | International | Conference               | on  | Virtual Execution | Environments,  | pages  |
References
144–153.ACMPress,2006.
| [1] LuaJIT | roadmap | 2008 | - http://lua-users.org/lists/lua-l/2008- |     |     |     |     |     |     |     |     |     |
| ---------- | ------- | ---- | ---------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
02/msg00051.html. [12] C.Garrett,J.Dean,D.Grove,andC.Chambers. Measurementand
ApplicationofDynamicReceiverClassDistributions.1994.
| [2] Mozilla | — Firefox | web browser | and | Thunderbird | email | client - |                                                 |     |     |     |     |             |
| ----------- | --------- | ----------- | --- | ----------- | ----- | -------- | ----------------------------------------------- | --- | --- | --- | --- | ----------- |
|             |           |             |     |             |       |          | [13] J.Ha,M.R.Haghighat,S.Cong,andK.S.McKinley. |     |     |     |     | Aconcurrent |
http://www.mozilla.com.
|     |     |     |     |     |     |     | trace-based | just-in-time | compiler | for javascript. | Dept.of | Computer |
| --- | --- | --- | --- | --- | --- | --- | ----------- | ------------ | -------- | --------------- | ------- | -------- |
[3] SPECJVM98-http://www.spec.org/jvm98/. Sciences,TheUniversityofTexasatAustin,TR-09-06,2009.
| [4] SpiderMonkey |     | (JavaScript-C) |     |     | Engine | -   |                                         |     |     |     |     |     |
| ---------------- | --- | -------------- | --- | --- | ------ | --- | --------------------------------------- | --- | --- | --- | --- | --- |
|                  |     |                |     |     |        |     | [14] B.McCloskey.Personalcommunication. |     |     |     |     |     |
http://www.mozilla.org/js/spidermonkey/.
|     |     |     |     |     |     |     | [15] I.PiumartaandF.Riccardi.Optimizingdirectthreadedcodebyselec- |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ----------------------------------------------------------------- | --- | --- | --- | --- | --- |
[5] Surfin’Safari-BlogArchive-AnnouncingSquirrelFishExtreme-
|     |     |     |     |     |     |     | tiveinlining. | InProceedingsoftheACMSIGPLAN1998conference |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------------- | ------------------------------------------ | --- | --- | --- | --- |
http://webkit.org/blog/214/introducing-squirrelfish-extreme/.
|             |           |            |             |            |     |             | on Programming | language |     | design and implementation, |     | pages 291– |
| ----------- | --------- | ---------- | ----------- | ---------- | --- | ----------- | -------------- | -------- | --- | -------------------------- | --- | ---------- |
| [6] A. Aho, | R. Sethi, | J. Ullman, | and M. Lam. | Compilers: |     | Principles, |                |          |     |                            |     |            |
300.ACMNewYork,NY,USA,1998.
techniques,andtools,2006.
|     |     |     |     |     |     |     | [16] A. Rigo. | Representation-Based |     | Just-In-time | Specialization | and the |
| --- | --- | --- | --- | --- | --- | --- | ------------- | -------------------- | --- | ------------ | -------------- | ------- |
[7] V. Bala, E. Duesterwald, and S. Banerjia. Dynamo: A transparent PsycoPrototypeforPython.InPEPM,2004.
| dynamicoptimizationsystem. |     |     | InProceedingsoftheACMSIGPLAN |     |     |     |                |             |          |                 |     |              |
| -------------------------- | --- | --- | ---------------------------- | --- | --- | --- | -------------- | ----------- | -------- | --------------- | --- | ------------ |
|                            |     |     |                              |     |     |     | [17] M. Salib. | Starkiller: | A Static | Type Inferencer | and | Compiler for |
ConferenceonProgrammingLanguageDesignandImplementation,
Python.InMaster’sThesis,2004.
pages1–12.ACMPress,2000.
|                                             |     |     |     |     |                   |     | [18] T.Suganuma,T.Yasue,andT.Nakatani. |     |     |                       | ARegion-BasedCompila- |     |
| ------------------------------------------- | --- | --- | --- | --- | ----------------- | --- | -------------------------------------- | --- | --- | --------------------- | --------------------- | --- |
| [8] M.Berndl,B.Vitale,M.Zaleski,andA.Brown. |     |     |     |     | ContextThreading: |     |                                        |     |     |                       |                       |     |
|                                             |     |     |     |     |                   |     | tionTechniqueforDynamicCompilers.      |     |     | ACMTransactionsonPro- |                       |     |
aFlexibleandEfficientDispatchTechniqueforVirtualMachineIn-
grammingLanguagesandSystems(TOPLAS),28(1):134–174,2006.
| terpreters. | InCodeGenerationandOptimization,2005.CGO2005. |     |     |     |     |     |                  |       |        |                  |       |             |
| ----------- | --------------------------------------------- | --- | --- | --- | --- | --- | ---------------- | ----- | ------ | ---------------- | ----- | ----------- |
|             |                                               |     |     |     |     |     | [19] M. Zaleski, | A. D. | Brown, | and K. Stoodley. | YETI: | A graduallY |
InternationalSymposiumon,pages15–26,2005.
|                 |     |           |                |            |     |          | Extensible | Trace Interpreter. |     | In Proceedings | of  | the International |
| --------------- | --- | --------- | -------------- | ---------- | --- | -------- | ---------- | ------------------ | --- | -------------- | --- | ----------------- |
| [9] C. Chambers | and | D. Ungar. | Customization: | Optimizing |     | Compiler |            |                    |     |                |     |                   |
ConferenceonVirtualExecutionEnvironments,pages83–93.ACM
| Technology | for SELF, | a Dynamically-Typed |     | O   | bject-Oriented | Pro- |     |     |     |     |     |     |
| ---------- | --------- | ------------------- | --- | --- | -------------- | ---- | --- | --- | --- | --- | --- | --- |
Press,2007.
| gramming | Language. | In Proceedings | of  | the ACM | SIGPLAN | 1989 |     |     |     |     |     |     |
| -------- | --------- | -------------- | --- | ------- | ------- | ---- | --- | --- | --- | --- | --- | --- |
ConferenceonProgrammingLanguageDesignandImplementation,
pages146–160.ACMNewYork,NY,USA,1989.
478