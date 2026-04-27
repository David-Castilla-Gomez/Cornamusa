|     |     |                   |     | One | VM              | to Rule |     | Them        | All |     |              |     |     |     |     |
| --- | --- | ----------------- | --- | --- | --------------- | ------- | --- | ----------- | --- | --- | ------------ | --- | --- | --- | --- |
|     |     |                   |     | ∗   |                 |         | ∗   |             |     | †   |              |     | †   |     |     |
|     |     | ThomasWu¨rthinger |     |     |                 |         |     | AndreasWo¨ß |     |     |              |     |     |     |     |
|     |     |                   |     |     | ChristianWimmer |         |     |             |     |     | LukasStadler |     |     |     |     |
|     |     |                   | †   |     | †               |         |     | §           |     |     | ∗            |     |     |     | ∗   |
GillesDuboscq ChristianHumer GregorRichards DougSimon MarioWolczko
| ∗   |     | †   |     |     |     |     |     |     |     |     | §   |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
OracleLabs InstituteforSystemSoftware,JohannesKeplerUniversityLinz,Austria S3Lab,PurdueUniversity
|     |     | {thomas.wuerthinger, |         |          |                   |                             |             | mario.wolczko}@oracle.com |     |               |     |     |     |     |     |
| --- | --- | -------------------- | ------- | -------- | ----------------- | --------------------------- | ----------- | ------------------------- | --- | ------------- | --- | --- | --- | --- | --- |
|     |     |                      |         |          | christian.wimmer, |                             | doug.simon, |                           |     |               |     |     |     |     |     |
|     |     |                      | {woess, | stadler, | duboscq,          | christian.humer}@ssw.jku.at |             |                           |     | gr@purdue.edu |     |     |     |     |     |
Abstract These implementations can be characterized in the fol-
lowingway:
| Building | high-performance |     | virtual | machines | is a | complex |     |     |     |     |     |     |     |     |     |
| -------- | ---------------- | --- | ------- | -------- | ---- | ------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
and expensive undertaking; many popular languages still • Their performance on typical applications is within a
havelow-performanceimplementations.Wedescribeanew
smallmultiple(1-3x)ofthebeststaticallycompiledcode
| approach | to virtual | machine | (VM) | construction | that | amor- |     |      |            |          |     |         |       |        |      |
| -------- | ---------- | ------- | ---- | ------------ | ---- | ----- | --- | ---- | ---------- | -------- | --- | ------- | ----- | ------ | ---- |
|          |            |         |      |              |      |       | for | most | equivalent | programs |     | written | in an | unsafe | lan- |
tizes much of the effort in initial construction by allowing guagesuchasC.
| new languages |     | to be implemented |     | with | modest additional |     |     |     |     |     |     |     |     |     |     |
| ------------- | --- | ----------------- | --- | ---- | ----------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
•
Theyareusuallywritteninanunsafe,systemsprogram-
effort.Theapproachreliesonabstractsyntaxtree(AST)in-
minglanguage(CorC++).
terpretationwhereanodecanrewriteitselftoamorespecial-
izedormoregeneralnode,togetherwithanoptimizingcom- • Theirimplementationishighlycomplex.
piler that exploits the structure of the interpreter. The com- • Theyimplementasinglelanguage,orprovideabytecode
pilerusesspeculativeassumptionsanddeoptimizationinor-
interfacethatadvantagesanarrowsetoflanguagestothe
dertoproduceefficientmachinecode.Ourinitialexperience
detrimentofotherlanguages.
| suggests | that high | performance |     | is attainable | while | preserv- |     |     |     |     |     |     |     |     |     |
| -------- | --------- | ----------- | --- | ------------- | ----- | -------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
ing a modular and layered architecture, and that new high- Incontrast,therearenumerouslanguagesthatarepopu-
performance language implementations can be obtained by lar, have been around for about 20 years, and yet still have
writinglittlemorethanastylizedinterpreter. noimplementationsthatevenapproachthislevelofperfor-
|                                 |     |     |     |                   |     |     | mance. | Examples   |     | are PHP, | Python, | Ruby,    | R,       | Perl, | MAT-   |
| ------------------------------- | --- | --- | --- | ----------------- | --- | --- | ------ | ---------- | --- | -------- | ------- | -------- | -------- | ----- | ------ |
| CategoriesandSubjectDescriptors |     |     |     | D.3.4[Programming |     |     |        |            |     |          |         |          |          |       |        |
|                                 |     |     |     |                   |     |     | LAB,   | Smalltalk, | and | APL.     | The     | computer | language |       | bench- |
Languages]:Processors—Run-timeenvironments
|     |     |     |     |     |     |     | marks | game | [20] | provides | some | insights | into | the | perfor- |
| --- | --- | --- | --- | --- | --- | --- | ----- | ---- | ---- | -------- | ---- | -------- | ---- | --- | ------- |
manceofvariouslanguages.
| Keywords | Java; | JavaScript; | dynamic |     | languages; | virtual |     |     |     |     |     |     |     |     |     |
| -------- | ----- | ----------- | ------- | --- | ---------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
machine;languageimplementation;optimization JavaScript is in an intermediate state. Performance was
|     |     |     |     |     |     |     | lackluster | until | 2008, | but | significant | effort | has | since | been |
| --- | --- | --- | --- | --- | --- | --- | ---------- | ----- | ----- | --- | ----------- | ------ | --- | ----- | ---- |
1. Introduction investedinseveralcompetingimplementationstomakeper-
|     |     |     |     |     |     |     | formance | respectable. |     | We  | believe | that | the recent | history | of  |
| --- | --- | --- | --- | --- | --- | --- | -------- | ------------ | --- | --- | ------- | ---- | ---------- | ------- | --- |
High-performanceVMimplementationsforobject-oriented
JavaScriptexplainswhytheotherlanguageshavelesserper-
languageshaveexistedforabouttwentyyears,aspioneered
|     |     |     |     |     |     |     | formance: | until | industrial-scale |     |     | investment | becomes |     | avail- |
| --- | --- | --- | --- | --- | --- | --- | --------- | ----- | ---------------- | --- | --- | ---------- | ------- | --- | ------ |
bytheSelfVM[25],andhavebeeninwidespreaduseforfif-
|     |     |     |     |     |     |     | able, | the complexity |     | of a | traditional | high-performance |     |     | VM  |
| --- | --- | --- | --- | --- | --- | --- | ----- | -------------- | --- | ---- | ----------- | ---------------- | --- | --- | --- |
teenyears.Examplesarehigh-performanceJavaVMssuch
|     |     |     |     |     |     |     | is too | high | for small-scale |     | efforts | to  | build | and maintain |     |
| --- | --- | --- | --- | --- | --- | --- | ------ | ---- | --------------- | --- | ------- | --- | ----- | ------------ | --- |
asOracle’sJavaHotSpotVM[28]andIBM’sJavaVM[30],
|     |     |     |     |     |     |     | a high-performance |     |     | implementation |     | for | multiple | hardware |     |
| --- | --- | --- | --- | --- | --- | --- | ------------------ | --- | --- | -------------- | --- | --- | -------- | -------- | --- |
aswellasMicrosoft’sCommonLanguageRuntime(theVM
platformsandoperatingsystems.
ofthe.NETframework[42]).
|     |     |     |     |     |     |     | We  | present | a new | approach | and | architecture, |     | which | en- |
| --- | --- | --- | --- | --- | --- | --- | --- | ------- | ----- | -------- | --- | ------------- | --- | ----- | --- |
ablesimplementingawiderangeoflanguageswithinacom-
Permissiontomakedigitalorhardcopiesofallorpartofthisworkforpersonalor
|     |     |     |     |     |     |     | mon | framework, | reusing |     | many | components | (especially |     | the |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------- | ------- | --- | ---- | ---------- | ----------- | --- | --- |
classroomuseisgrantedwithoutfeeprovidedthatcopiesarenotmadeordistributed
forprofitorcommercialadvantageandthatcopiesbearthisnoticeandthefullcitation optimizingcompiler).Thisbringsexcellentperformancefor
onthefirstpage.Copyrightsforcomponentsofthisworkownedbyothersthanthe alllanguagesusingourframework.Ourapproachreliesona
author(s)mustbehonored.Abstractingwithcreditispermitted.Tocopyotherwise,or
rewrite1
republish,topostonserversortoredistributetolists,requirespriorspecificpermission framework that allows nodes to themselves during
and/orafee.Requestpermissionsfrompermissions@acm.org. interpretation, a speculatively optimizing compiler that can
Onward!2013,
October29–31,2013,Indianapolis,Indiana,USA.
Copyrightisheldbytheowner/author(s).PublicationrightslicensedtoACM.
1Weuse“noderewriting”inasensethatisdistinctfromrewritinginthe
ACM978-1-4503-2472-4/13/10/13/10...$15.00.
http://dx.doi.org/10.1145/2509578.2509581 contextofthelambdacalculus,formalsemantics,ortermrewritingsystems.

exploitthestructureofinterpreterswrittenusingthisframe-
|                              |                |     |           |      |                |     |      | Written by:  |     |                            |     |     | Written in:    |     |     |
| ---------------------------- | -------------- | --- | --------- | ---- | -------------- | --- | ---- | ------------ | --- | -------------------------- | --- | --- | -------------- | --- | --- |
| work, and                    | deoptimization |     | to revert | back | to interpreted |     | exe- | Application  |     |                            |     |     |                |     |     |
|                              |                |     |           |      |                |     |      |              |     | Guest Language Application |     |     | Guest Language |     |     |
| cutiononspeculationfailures. |                |     |           |      |                |     |      | Developer    |     |                            |     |     |                |     |     |
In this paper, we describe the overall approach and the Language  Guest Language Implementation
|     |     |     |     |     |     |     |     | Developer |     |     |     |     | Managed Host Language |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | --- | --- | --- | --------------------- | --- | --- |
structureofourimplementation,withparticularattentionto
theinteractionbetweentheinterpreter,thecompiler,andde- Managed Host Language
|     |     |     |     |     |     |     |     | VM Expert |     | Host Services |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | ------------- | --- | --- | --- | --- | --- |
or Unmanaged Language
| optimization. | Additionally, |     | we  | describe | the | variety | of lan- |     |     |     |     |     |     |     |     |
| ------------- | ------------- | --- | --- | -------- | --- | ------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
guage implementations we are undertaking, with specific OS Unmanaged Language
|     |     |     |     |     |     |     |     | OS Expert |     |     |     |     | (typically C or C++) |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | --- | --- | --- | -------------------- | --- | --- |
referencetotheiruniqueattributesandthechallengesposed.
Ourfocusisondynamicallytyped,imperativeprogramming Figure1. Systemstructureofaguestlanguageimplementa-
languagessuchasJavaScript,Ruby,andPython;aswellas tionutilizinghostservicestobuildahigh-performanceVM
languagesfortechnicalcomputingsuchasRandJ.Section6 fortheguestlanguage.
presentsdetailsonthelanguages.
Thispaperpresentsaforward-lookingviewpointonwork offermanybenefitsforapplicationsrunningontopofthem,
| in progress. | We  | have a | working | prototype | implementation |     |     |     |     |     |     |     |     |     |     |
| ------------ | --- | ------ | ------- | --------- | -------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
buttheymostlydonotutilizethesebenefitsforthemselves.
oftheinterpreterframeworkandthecompilationinfrastruc-
Incontrast,wewanttheVMsformanydifferentguestlan-
ture. A detailed description of the node rewriting appears guageswritteninamanagedhostlanguage.Onlytheguest-
| elsewhere | [72]; | a brief | summary | of  | that paper | appears | at  |     |     |     |     |     |     |     |     |
| --------- | ----- | ------- | ------- | --- | ---------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
languagespecificpartisimplementedanewbythelanguage
the beginning of Section 3. The AST and machine code of developer. A core of reusable host services are provided
the example presented in the subsequent sections are pro- bytheframework,suchasdynamiccompilation,automatic
ducedbyouractualsystem.Wearethereforeconvincedthat
|     |     |     |     |     |     |     |     | memory | management, |     | threads, | synchronization |     | primitives, |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | ----------- | --- | -------- | --------------- | --- | ----------- | --- |
theapproachcanbesuccessful.However,moreimplementa- andawell-definedmemorymodel.Figure1summarizesour
tionworkisnecessarytogetlargerindustry-standardbench-
systemstructure.
marksrunningformultiplelanguages.
|     |     |     |     |     |     |     |     | The | host services |     | can either | be  | written | in the managed |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------- | --- | ---------- | --- | ------- | -------------- | --- |
Our language implementation framework, called Truffle, host language or in an unmanaged language; our approach
| as well | as our compilation |     | infrastructure, |     | called | Graal, | are |     |     |     |     |     |     |     |     |
| ------- | ------------------ | --- | --------------- | --- | ------ | ------ | --- | --- | --- | --- | --- | --- | --- | --- | --- |
doesnotimposerestrictionsonthat.Section5presentsdif-
available as open source in an OpenJDK project [45]. We ferentapproachesforprovidingthehostservices.
| encourage | academic | and | open | source | community | contribu- |     |         |          |     |          |         |        |           |      |
| --------- | -------- | --- | ---- | ------ | --------- | --------- | --- | ------- | -------- | --- | -------- | ------- | ------ | --------- | ---- |
|           |          |     |      |        |           |           |     | For the | concrete |     | examples | in this | paper, | the guest | lan- |
tions,especiallyintheareaofnewinnovativelanguagefea-
guageisJavaScriptandthehostlanguageisasubsetofJava,
tures and language implementations. Language implemen- i.e.,wehaveaguestJavaScriptVMaswellasthehostser-
| tations that | are started |     | now, using | our | framework, |     | will au- |     |     |     |     |     |     |     |     |
| ------------ | ----------- | --- | ---------- | --- | ---------- | --- | -------- | --- | --- | --- | --- | --- | --- | --- | --- |
vicesimplementedinJava.However,wewanttostressthat
tomaticallyprofitfromourcompilationinfrastructurewhen the idea is language independent. Section 6 shows how to
itisfinished,soweenvisionmultiplelanguageimplementa-
|     |     |     |     |     |     |     |     | map key | features | of  | many | guest languages |     | to our | system. |
| --- | --- | --- | --- | --- | --- | --- | --- | ------- | -------- | --- | ---- | --------------- | --- | ------ | ------- |
tionsbeingdevelopedbythirdpartiessimultaneouslytoour
Wehavedesignedoursystemintheexpectationthatthehost
efforts.Insummary,thispapercontributesthefollowing: language is statically typed. This allows us to express type
• We present a new VM architecture and our implemen- specializations, i.e., to explicitly express the semantics of a
|     |     |     |     |     |     |     |     | guest language |     | operation | on  | a specific | type, | as well | as to |
| --- | --- | --- | --- | --- | --- | --- | --- | -------------- | --- | --------- | --- | ---------- | ----- | ------- | ----- |
tation,whichcanbeusedtoconstructhigh-performance
implementationsofawiderangeoflanguagesatmodest explicitlyexpresstypeconversionsinthe(possiblydynami-
callytyped)guestlanguage.
incrementalcost.
|     |     |     |     |     |     |     |     | Our | layered | approach | simplifies |     | guest | language | imple- |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------- | -------- | ---------- | --- | ----- | -------- | ------ |
•
Weshowhowthecombinationofnoderewritingduring
|                 |     |            |              |     |     |             |     | mentation.             | Host | services | factor | out      | common | parts found | in  |
| --------------- | --- | ---------- | ------------ | --- | --- | ----------- | --- | ---------------------- | ---- | -------- | ------ | -------- | ------ | ----------- | --- |
| interpretation, |     | optimizing | compilation, |     | and | deoptimiza- |     |                        |      |          |        |          |        |             |     |
|                 |     |            |              |     |     |             |     | every high-performance |      |          | VM,    | allowing | guest  | language    | de- |
tiondelivershighperformancefromaninterpreterwith- veloperstofocusonrequiredexecutionsemantics.However,
outrequiringalanguage-specificcompiler.
|     |     |     |     |     |     |     |     | the benefits | of  | the layering |     | must not | sacrifice | peak | perfor- |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------ | --- | ------------ | --- | -------- | --------- | ---- | ------- |
• Weshowhowfeaturesofavarietyofprogramminglan- manceoftheguestlanguage.Dynamiccompilationofguest
guagesmaponourframework. language code to machine code is therefore essential. Fig-
• ure 2 and 3 illustrate the key steps of our language imple-
| We describe |     | how our | system | supports |     | alternative | de- |     |     |     |     |     |     |     |     |
| ----------- | --- | ------- | ------ | -------- | --- | ----------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
mentationandoptimizationstrategy:
| ployment | strategies; |     | these | enhance | developer |     | produc- |     |     |     |     |     |     |     |     |
| -------- | ----------- | --- | ----- | ------- | --------- | --- | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
tivity and separate language-specific from language- • TheguestlanguageimplementationiswrittenasanAST
independentcomponents.
|     |     |     |     |     |     |     |     | interpreter. |     | We believe | that | implementing |     | an AST | inter- |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------ | --- | ---------- | ---- | ------------ | --- | ------ | ------ |
preterforalanguageisasimpleandintuitivewayofde-
2. SystemStructure
scribingthesemanticsofmanylanguages.Forexample,
High-performance VM implementations are still mostly the semantics for an addition are well encapsulated and
monolithicpiecesofsoftwaredevelopedinCorC++.VMs intuitivelydescribedinoneplace:theadditionnode.

|     |     |     |     |     | Node Rewriting         |     |     |     |     | Compilation using  |     |     |     |     |
| --- | --- | --- | --- | --- | ---------------------- | --- | --- | --- | --- | ------------------ | --- | --- | --- | --- |
|     |     | U   |     |     | for Profiling Feedback |     |     |     | G   | Partial Evaluation |     |     |     |     |
G
|     |     | U   | U   | Node Transitions |               |         |     | I   |     | G   |     |     | I G |     |
| --- | --- | --- | --- | ---------------- | ------------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
|     |     |     |     |                  | Uninitialized | Integer |     |     |     |     |     | I   | I   |     |
U
|     | U                   | U   |     |        |        | I   |     | I               | I   |     |     |               |     |     |
| --- | ------------------- | --- | --- | ------ | ------ | --- | --- | --------------- | --- | --- | --- | ------------- | --- | --- |
|     |                     |     |     | S      | D      |     |     |                 |     |     |     |               |     |     |
|     | AST Interpreter     |     |     |        |        |     |     | AST Interpreter |     |     |     |               |     |     |
|     |                     |     |     | String | Double |     |     |                 |     |     |     | Compiled Code |     |     |
|     | Uninitialized Nodes |     |     |        |        |     |     | Rewritten Nodes |     |     |     |               |     |     |
G
Generic
Figure2. Noderewritingforprofilingfeedbackandpartialevaluationleadtoaggressivelyspecializedmachinecode.
|     |     |     | Deoptimization     |                                                                  |     | Node Rewriting to Update  |     |     |     | Recompilation using |     |     |     |     |
| --- | --- | --- | ------------------ | ---------------------------------------------------------------- | --- | ------------------------- | --- | --- | --- | ------------------- | --- | --- | --- | --- |
|     |     |     |                    |                                                                  | G   |                           |     |     | G   |                     |     |     |     |     |
|     |     |     | to AST Interpreter |                                                                  |     | Profiling Feedback        |     |     |     | Partial Evaluation  |     |     |     |     |
|     |     | G   |                    |                                                                  |     |                           |     |     |     |                     |     |     | G   |     |
|     |     | I G |                    |                                                                  | I   | G                         |     | D   |     | G                   |     |     | D G |     |
|     | I   | I   |                    |                                                                  |     |                           |     |     |     |                     |     | I   | D   |     |
|     |     |     |                    |                                                                  | I I |                           |     | I   | D   |                     |     |     |     |     |
|     |     |     | Figure3.           | DeoptimizationbacktotheASTinterpreterhandlesspeculationfailures. |     |                           |     |     |     |                     |     |     |     |     |
•
Noderewritingintheinterpretercapturesprofilinginfor- tation, the nodes then perform the necessary rewrites to
mation,e.g.,dynamictypeinformation(seeSection3for incorporaterevisedtypeinformationintotheAST,which
details). During interpretation, a node can replace itself canbecompiledagainusingpartialevaluation.
| at  | its parent | with | a different | node.            | This allows | a node     |     |           |     |          |         |         |          |       |
| --- | ---------- | ---- | ----------- | ---------------- | ----------- | ---------- | --- | --------- | --- | -------- | ------- | ------- | -------- | ----- |
| to  | specialize | on   | a subset    | of the semantics | of          | a particu- |     |           |     |          |         |         |          |       |
|     |            |      |             |                  |             |            |     | Note that | at  | no point | was the | dynamic | compiler | modi- |
larguestlanguageoperation.Thisrewritingincorporates
fiedtounderstandthesemanticsoftheguestlanguage;these
profilingfeedbackintheAST.Countersmeasureexecu-
existsolelyinthehigh-levelcodeoftheinterpreterandrun-
tionfrequenciesandtherateofnoderewrites. time. A guest language developer who operates within our
•
When the compiler is invoked to compile a part of the interpretationframeworkgetsahigh-performancelanguage
application, it uses the extant ASTs (with profiling and implementation, with no need to understand dynamic com-
frequency information) to perform an automatic partial pilation. Subsequent sections illustrate what kinds of trans-
evaluation of the AST interpreter (see Section 4 for de- formations are required to enable the compiler to generate
tails).Forthis,theASTisassumedtobeinastablestate efficientmachinecode.
wheresubsequentrewritesareunlikely,althoughnotpro- The example AST in Figure 2 consists of 5 nodes. The
hibited.Partialevaluation,i.e.,compilationwithaggres- first snapshot on the left side shows the tree before execu-
sive method inlining, eliminates the interpreter dispatch tion,wherenotypeinformationisavailableyet.Duringex-
codeandproducesoptimizedmachinecodefortheguest ecution,thenodesarereplacedwithtypednodesaccording
| language. |     |     |     |     |     |     | totypesseenatruntime.Thetypetransitionsshowninthe |     |     |     |     |     |     |     |
| --------- | --- | --- | --- | --- | --- | --- | ------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
• The parts of the interpreter code responsible for node figure model the guest language (JavaScript) data types us-
inghostlanguage(Java)types.JavaScriptnumbersarerep-
| rewriting |     | are omitted | from | compilation. | Branches | that |     |     |     |     |     |     |     |     |
| --------- | --- | ----------- | ---- | ------------ | -------- | ---- | --- | --- | --- | --- | --- | --- | --- | --- |
resentedbytheJavatypedouble,withtheoptimizationthat
performrewritingarenotcompiled,butinsteadcausede-
|              |     |       |      |            |              |         | the | Java type | integer | can | be used | for computations |     | that do |
| ------------ | --- | ----- | ---- | ---------- | ------------ | ------- | --- | --------- | ------- | --- | ------- | ---------------- | --- | ------- |
| optimization |     | [27]. | This | results in | machine code | that is |     |           |         |     |         |                  |     |         |
notoverflowthe32-bitsignedintegerrange.Observingthat
aggressivelyspecializedforthetypesandvaluesencoun-
teredduringinterpretation. three nodes are typed to integer, compilation by using par-
|     |     |     |     |     |     |     | tial | evaluation | generates |     | integer-specialized |     | code | for these |
| --- | --- | --- | --- | --- | --- | --- | ---- | ---------- | --------- | --- | ------------------- | --- | ---- | --------- |
•
In case that a specialization subsequently fails, deopti- nodes.Notethatweusetypetransitionsonlyasanintuitive
mizationdiscardstheoptimizedmachinecodeandreverts
examplehere.Noderewritingisusedforprofilingfeedback
| execution |     | back to | the AST | interpreter. | During | interpre- |     |     |     |     |     |     |     |     |
| --------- | --- | ------- | ------- | ------------ | ------ | --------- | --- | --- | --- | --- | --- | --- | --- | --- |
ingeneralandnotrestrictedtotypetransitions.

Figure3continuestheexample.Assumethatacomputa- NodereplacementallowstheASTinterpretertoautomat-
tion overflows the integer range. The code compiled can- ically incorporate profiling feedback while executing. This
not handle this case, so deoptimization reverts execution profiling feedback is expressed as the current state of the
back to the AST interpreter. During interpretation, the in- AST.Concretepossibilitiesincludebutarenotlimitedto:
| teger nodes | rewrite | themselves |     | to double | nodes. |     | After this |                     |     |           |     |            |     |           |     |
| ----------- | ------- | ---------- | --- | --------- | ------ | --- | ---------- | ------------------- | --- | --------- | --- | ---------- | --- | --------- | --- |
|             |         |            |     |           |        |     |            | TypeSpecialization: |     | Operators |     | in dynamic |     | languages | can |
change,theASTiscompiledagain,thistimeproducingdou-
|     |     |     |     |     |     |     |     | often | be applied | to a | wide | variety | of operand |     | types. A |
| --- | --- | --- | --- | --- | --- | --- | --- | ----- | ---------- | ---- | ---- | ------- | ---------- | --- | -------- |
ble-specializedcode.
|     |     |     |     |     |     |     |     | full implementation |     |     | of such | an operator |     | must therefore |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------------- | --- | --- | ------- | ----------- | --- | -------------- | --- |
applyseveraldynamictypecheckstotheoperandsbefore
3. Self-optimization
|     |     |     |     |     |     |     |     | choosing | the | appropriate | version | of  | the operator |     | to exe- |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | --- | ----------- | ------- | --- | ------------ | --- | ------- |
A guest language developer using our system writes a spe- cute.However,foreachparticularoperatorinstanceina
cifickindofASTinterpreterinamanagedlanguage.Aguest guestlanguageprogram,itislikelythattheoperandsal-
languagefunctionhasarootnodewithanexecutemethod wayshavethesametypes.TheASTinterpretercanthere-
| returning | the result | of  | the function. | Every | node | in  | the AST |                |     |        |       |        |          |     |          |
| --------- | ---------- | --- | ------------- | ----- | ---- | --- | ------- | -------------- | --- | ------ | ----- | ------ | -------- | --- | -------- |
|           |            |     |               |       |      |     |         | fore speculate |     | on the | types | of the | operands | and | only in- |
hasalistofchildnodes.Allnodesexcepttherootnodealso clude the code for one case. If the speculation fails, the
haveasingleassociatedparentnode.Theparentnodecalls operatornodereplacesitselfwithamoregeneralversion.
executemethodsofitschildnodestocomputeitsownre-
|     |     |     |     |     |     |     |     | PolymorphicInlineCaches: |     |     | Our | system | supports |     | poly- |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | --- | --- | ------ | -------- | --- | ----- |
sult.Non-localcontrolflow(e.g.,break,continue,return)in
morphicinlinecaches[26]bychainingnodesrepresent-
| the guest | language | is  | modeled | using | host language |     | excep- |     |     |     |     |     |     |     |     |
| --------- | -------- | --- | ------- | ----- | ------------- | --- | ------ | --- | --- | --- | --- | --- | --- | --- | --- |
ingentriesinthecache.Foreverynewentryinthecache,
tions.
anewnodeisaddedtothetree.Thenodecheckswhether
theentrymatches.Theniteitherproceedswiththeoper-
3.1 NodeRewriting
ationspecializedforthisentryordelegatesthehandling
Duringexecution,anodecanreplaceitselfatitsparentwith
|     |     |     |     |     |     |     |     | of the | operation | to  | the next | node | in the | chain. | When |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | --------- | --- | -------- | ---- | ------ | ------ | ---- |
adifferentnode.Thisallowsanodetospecializeonasubset the chain reaches a certain predefined length (e.g., the
| of the semantics |     | of a | particular | guest | language | operation. |     |         |       |        |           |       |          |        |      |
| ---------------- | --- | ---- | ---------- | ----- | -------- | ---------- | --- | ------- | ----- | ------ | --------- | ----- | -------- | ------ | ---- |
|                  |     |      |            |       |          |            |     | desired | cache | size), | the whole | chain | replaces | itself | with |
If its own handling of the operation cannot succeed for the onenoderesponsibleforhandlingthefullymegamorphic
| current | operands | or system |     | environment, |     | a node | replaces | case. |     |     |     |     |     |     |     |
| ------- | -------- | --------- | --- | ------------ | --- | ------ | -------- | ----- | --- | --- | --- | --- | --- | --- | --- |
itselfandletsthenewnodereturntheappropriateresult.
|           |             |           |         |                |           |             |            | ResolvingOperations: |      |          | Using       | rewriting, | a guest | language   |      |
| --------- | ----------- | --------- | ------- | -------------- | --------- | ----------- | ---------- | -------------------- | ---- | -------- | ----------- | ---------- | ------- | ---------- | ---- |
| Node      | replacement |           | depends | on the         | following | conditions. |            |                      |      |          |             |            |         |            |      |
|           |             |           |         |                |           |             |            | operation            | that | includes | a resolving |            | step    | upon first | exe- |
| The guest | language    | developer |         | is responsible |           | for         | fulfilling |                      |      |          |             |            |         |            |      |
cutionreplacesitselfwitharesolvedversionoftheoper-
them,thereisnoautomaticenforcementorverification.
ation.Thisway,itavoidssubsequentchecks,atechnique
thatisrelatedtobytecodequickening[11].
| Completeness: |           | Although | a node  | may      | handle     | only | a subset |     |     |     |     |     |     |     |     |
| ------------- | --------- | -------- | ------- | -------- | ---------- | ---- | -------- | --- | --- | --- | --- | --- | --- | --- | --- |
| of the        | semantics | of       | a guest | language | operation, |      | it must  |     |     |     |     |     |     |     |     |
Rewritingsplitstheimplementationofanoperationinto
providerewritesforallcasesthatitdoesnothandleitself.
|     |     |     |     |     |     |     |     | multiple | nodes. | In general, | this | should | be considered |     | if the |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ------ | ----------- | ---- | ------ | ------------- | --- | ------ |
Finiteness: Afterafinitenumberofnodereplacements,the followingtwoconditionsaremet:theimplementationcovers
operation must end up in a state that handles the full only a subset of the operator’s semantics; and that subset
semanticswithoutfurtherrewrites.Inotherwords,there
|     |     |     |     |     |     |     |     | is likely | sufficient | for the | application |     | of the | operation | at a |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ---------- | ------- | ----------- | --- | ------ | --------- | ---- |
must be a generic implementation of the operation that specificguestlanguageprogramlocation.
canhandleallpossibleinputsitself. As a consequence of rewriting, the actual host-language
Locality: Arewritemayonlyhappenlocallyforthecurrent code that is executed changes dynamically when the guest-
|     |     |     |     |     |     |     |     | language | interpreter | executes |     | a guest-language |     | method— |     |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ----------- | -------- | --- | ---------------- | --- | ------- | --- |
nodewhileitisexecuting.However,aspartofthisrewrite
|        |     |         |           |     |           |     |          | usually towards |     | a faster, | more | specialized | version |     | on suc- |
| ------ | --- | ------- | --------- | --- | --------- | --- | -------- | --------------- | --- | --------- | ---- | ----------- | ------- | --- | ------- |
| a node | may | replace | or change | the | structure | of  | its com- |                 |     |           |      |             |         |     |         |
cessiveexecutions.Becauseofthis,ourinterpreterhasgood
pletesubtree.
|     |     |     |     |     |     |     |     | performance | compared |     | to other | interpreter |     | implementa- |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------- | -------- | --- | -------- | ----------- | --- | ----------- | --- |
Whenaparentnodecallstheexecutemethodofachild tions [72]. However, we cannot compete with the perfor-
node,theparentnodemayprovideinformationabouttheex- mance of compiled code. The main overhead is due to dy-
pected constraints on the return value. The child node may namicdispatch.WeaddressthisprobleminSection4
| either fulfill | the | expected | constraints |     | or provide |     | the result |     |     |     |     |     |     |     |     |
| -------------- | --- | -------- | ----------- | --- | ---------- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- |
3.2 BoxingElimination
| value via | an exception. |     | After | catching | such | an exception, |     |     |     |     |     |     |     |     |     |
| --------- | ------------- | --- | ----- | -------- | ---- | ------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
a parent node must replace itself with a new node that no Thestandardexecutemethodofanodealwaysreturnsan
longer expects the child node to comply with these con- Objectvalue.However,anodemay,e.g.,offeraspecialized
straints. This allows a parent node to communicate to its executeInt method, whose return type is the primitive
child node its preferred kind of returned values (see Sec- integer type.Aparentnodecancallthismethodifitwants
tion3.4foranexample). to receive an integer value. The child node can then either

| function | sum(n) {  |     |     |     | Function: sum |     |     |     |     |
| -------- | --------- | --- | --- | --- | ------------- | --- | --- | --- | --- |
var sum = 0;
| for (var | i = 1; i < n; i++) {  |     |     |     | Block |     |     |     |     |
| -------- | --------------------- | --- | --- | --- | ----- | --- | --- | --- | --- |
sum += i;
| }      |       |     |     |     |     | Write: 2 (n)   | Argument: 0 (n) |     |     |
| ------ | ----- | --- | --- | --- | --- | -------------- | --------------- | --- | --- |
| return | sum;  |     |     |     |     |                |                 |     |     |
|        |       |     |     |     |     | Write: 0 (sum) | IConstant: 0    |     |     |
}
|          |                                |     |     |     |     | Write: 1 (i) | IConstant: 1 |     |     |
| -------- | ------------------------------ | --- | --- | --- | --- | ------------ | ------------ | --- | --- |
| Figure4. | JavaScriptcodeofexamplemethod. |     |     |     |     |              |              |     |     |
For
Read: 1 (i)
left
returnthevalueasaninteger,orifthisthisnotpossible,box condition LessThan
thevalueinanobjectandwrapitinanexception.Theparent right Read: 2 (n)
| node catches | this exception | and replaces | itself with | a node |     | body |     |     |     |
| ------------ | -------------- | ------------ | ----------- | ------ | --- | ---- | --- | --- | --- |
Write: 0  (sum)
Read: 0  (sum)
| thatnolongercallstheexecuteIntmethod. |     |     |     |     |     |     |     | left |     |
| ------------------------------------- | --- | --- | --- | --- | --- | --- | --- | ---- | --- |
Add
| This technique | can be | used to avoid | any sorts | of boxing |     |     |     |     |     |
| -------------- | ------ | ------------- | --------- | --------- | --- | --- | --- | --- | --- |
right Read: 1 (i)
incaseachildnodealwaysproducesaprimitivevalue.The
increment
Increment: 1 (i)
| parent node | no longer | needs a type | check, type | cast, or |     |     |     |     |     |
| ----------- | --------- | ------------ | ----------- | -------- | --- | --- | --- | --- | --- |
unboxingoperationafterreceivingthevalueofthechildon Read: 0 (sum)
| the normal | path of execution. | The exceptional | behavior | is  |     |          |                         |     |     |
| ---------- | ------------------ | --------------- | -------- | --- | --- | -------- | ----------------------- | --- | --- |
|            |                    |                 |          |     |     | Figure5. | ExampleASTafterparsing. |     |     |
movedtoacatchblock.
3.3 TreeCloning
|     |     |     |     |     | perform | computations | that fit | into the value range | of 32- |
| --- | --- | --- | --- | --- | ------- | ------------ | -------- | -------------------- | ------ |
One problem with dynamic runtime feedback is the pollu- bitsignedintegersasinteger operations(representedinour
tionofthefeedbackduetodifferentcallersusingamethod examplebytheprefixI).
indifferentways.Inoursystem,everycallercanpotentially Assume that the method sum is first called with a small
cause a rewrite of AST nodes to a less specialized version. value of the parameter n so that sum fits in the range of
An operation has to handle the union of the semantics re- integer.Duringthefirstexecutionofsum,thenodesreplace
quiredbyeverycaller.Thisworksagainstourgoalofhaving themselves with nodes specialized for the type integer. For
every node handle only the smallest, required subset of the example, the Add node first evaluates its left child (the
semanticsofafulloperation. read of the local variable sum) and right child (the read
WeusecloningoftheASTtomitigatethiseffect.Every of the local variable i), using the execute methods of the
nodehastheabilitytocreateacopyofitself.Bycloningthe children. Both execute methods return a boxed Integer
ASTofaguestlanguagemethodforacallsite,weavoidthe object.Therefore,theAddnodereplacesitselfwithanIAdd
problemofprofilingfeedbackpollutionforthatmethod.The node. This rewriting process involves instantiating a new
cloningincreasesthenumberofASTnodesinthesystem,so tree node, i.e., a new object where the execute method
we have to be selective in choosing the targets for cloning. is implemented differently. The IAdd node will be used
For this decision, we use heuristics that include the current for subsequent executions. For the current execution, the
specialization state of the AST as well as the invocation Add node unboxes the Integer object to primitive integer
countofamethod. values,performstheaddition,andreturnsaboxedInteger
object.
3.4 Example The new IAdd node can only add integer numbers and
In order to demonstrate the abstract ideas in this paper, triggers another replacement if a child returns a differ-
we use a concrete JavaScript method as a running example ently typed value. It communicates its expectation to re-
throughout the paper. Although the example shows only a ceive an integer value to its children by calling a special-
simplesubsetofJavaScript,itsufficestoexplainmanyofour ized executeInt method instead of the generic execute
concepts.Figure4showstheJavaScriptsourcecodeofour method. This method has a primitive integer return type,
examplemethod.Itaddsupallnon-negativeintegersbelow i.e.,subsequentexecutionsdonotrequireboxing.Ifachild
aprovidedmaximumandreturnstheresult. cannot comply by returning an integer value, it must wrap
Figure5showstheASTofthemethodafterparsing.The the return value in an exception and return it on this alter-
loop and other operations from the source code are clearly native path. This UnexpectedResult exception triggers a
visible. Uninitialized operations are shown with no type replacementoftheIAddnode.
prefixinthenodeoperation.Onlyconstantsaretypedatthis Figure 6 shows the implementation of the IAdd node.
state,sinceitisknownwhetherornottheyfitintothevalue Note that the Math.addExact method is a plain Java util-
range of integer. Although the JavaScript language only ity method added to the upcoming JDK 8; it throws an
defines floating point numbers (represented in our example ArithmeticExceptioninsteadofreturninganoverflowed
by the prefix D), it is faster and more space efficient to result. The implementation of the rewrite method, which

| class | IAddNode extends               |                                          |     | BinaryNode { |                    |     |     | Function: sum |                 |                 |
| ----- | ------------------------------ | ---------------------------------------- | --- | ------------ | ------------------ | --- | --- | ------------- | --------------- | --------------- |
|       | int executeInt(Frame f) throws |                                          |     |              | UnexpectedResult { |     |     |               |                 |                 |
|       | int                            | a;                                       |     |              |                    |     |     | Block         |                 |                 |
|       | try                            | {                                        |     |              |                    |     |     |               |                 |                 |
|       | a = left.executeInt(f);        |                                          |     |              |                    |     |     |               | IWrite: 2 (n)   | Argument: 0 (n) |
|       | } catch                        | (UnexpectedResult ex) {                  |     |              |                    |     |     |               |                 |                 |
|       |                                |                                          |     |              |                    |     |     |               | IWrite: 0 (sum) | IConstant: 0    |
|       | throw                          | rewrite(f, ex.result, right.execute(f)); |     |              |                    |     |     |               |                 |                 |
}
|     |     |     |     |     |     |     |     |     | IWrite: 1 (i) | IConstant: 1 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------- | ------------ |
|     | int | b;  |     |     |     |     |     |     |               |              |
For
|     | try | {   |     |     |     |     |     |     |     | IRead: 1 (i) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------ |
left
|     | b = right.executeInt(f); |                           |     |     |     |     |     |     | condition ILessThan |                    |
| --- | ------------------------ | ------------------------- | --- | --- | --- | --- | --- | --- | ------------------- | ------------------ |
|     | } catch                  | (UnexpectedResult ex) {   |     |     |     |     |     |     |                     | right IRead: 2 (n) |
|     | throw                    | rewrite(f, a, ex.result); |     |     |     |     |     |     | body                |                    |
IWrite: 0  (sum)
} IRead: 0  (sum)
left
IAdd
|     | try    | {                    |     |     |     |     |     |     |           | right IRead: 1 (i) |
| --- | ------ | -------------------- | --- | --- | --- | --- | --- | --- | --------- | ------------------ |
|     | return | Math.addExact(a, b); |     |     |     |     |     |     | increment |                    |
IIncrement: 1 (i)
|     | } catch | (ArithmeticException ex) { |     |     |     |     |     |     |     |     |
| --- | ------- | -------------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
|     | throw   | rewrite(f, a, b);          |     |     |     |     |     |     |     |     |
IRead: 0 (sum)
}
}
|        |          |                                      |             |         |          |                |     |               | Figure7. ExampleASTspecializedtointeger. |     |
| ------ | -------- | ------------------------------------ | ----------- | ------- | -------- | -------------- | --- | ------------- | ---------------------------------------- | --- |
|        | Figure6. | Implementationofintegeradditionnode. |             |         |          |                |     |               |                                          |     |
| is not | shown    | in                                   | the figure, | creates | a new    | node, replaces | the | Function: sum |                                          |     |
| IAdd   | node     | with                                 | this new    | node,   | performs | the addition,  | and |               |                                          |     |
Block
| returns | the | non-integer |     | result of | this addition | wrapped | in a |     |     |     |
| ------- | --- | ----------- | --- | --------- | ------------- | ------- | ---- | --- | --- | --- |
newUnexpectedResultexception.
|     |        |         |     |           |     |           |        |     | IWrite: 2 (n)   | Argument: 0 (n) |
| --- | ------ | ------- | --- | --------- | --- | --------- | ------ | --- | --------------- | --------------- |
|     | Figure | 7 shows | the | AST after | the | execution | of the |     |                 |                 |
|     |        |         |     |           |     |           |        |     | DWrite: 0 (sum) | IConstant: 0    |
method,withallarithmeticoperationstypedtointeger.The
|     |     |     |     |     |     |     |     |     | IWrite: 1 (i) | IConstant: 1 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------- | ------------ |
treeisinastablestateandcanbecompiled(seeSection4.6).
Assumenowthatthemethodsumiscalledwithalarger
For
| value | of n, | still | in the | range of integer, |     | but causing | sum to |     |     |     |
| ----- | ----- | ----- | ------ | ----------------- | --- | ----------- | ------ | --- | --- | --- |
left IRead: 1 (i)
condition ILessThan
overflowtodouble.TheIAddnodedetectsthisoverflow,but
right IRead: 2 (n)
| is not | able | to perform |     | the computation |     | using type | double. |     |     |     |
| ------ | ---- | ---------- | --- | --------------- | --- | ---------- | ------- | --- | --- | --- |
body
DWrite: 0  (sum)
Instead, it replaces itself with a DAdd node. Subsequently, DRead: 0  (sum)
left
the nodes that write and read the local variable sum also DAdd
replace themselves. Figure 8 shows the AST after all the right IRead: 1 (i)
increment
type changes have been performed. The tree is in a stable IIncrement: 1 (i)
stateagainandcanbecompiled.
DRead: 0 (sum)
3.5 DSLforSpecializations
|     |          |       |     |         |             |     |            | Figure8.  | ExampleASTwithlocalvariablesumspecialized |     |
| --- | -------- | ----- | --- | ------- | ----------- | --- | ---------- | --------- | ----------------------------------------- | --- |
| We  | continue | using | the | example | from Figure | 4.  | Instead of | todouble. |                                           |     |
lookingatthewholeloop,wewanttoconcentrateontheAdd
operation.TheAddoperationreplacesitselfwithmorespe-
cializedformswhenexecuted.Implementingsuchbehavior useaJavaannotationprocessoratcompiletimetogenerate
inthehostlanguageisarepetitiveandredundanttaskifper- additionalJavacodefromtheannotations.
formed for many operations. Most code shown in Figure 6 Figure 9 illustrates the development process: Our Java
simply handles the rewriting and does not depend on guest annotations are used in the source code (step 1). When the
languagesemantics. Javacompilerisinvokedonthesourcecode(step2),itsees
Adomain-specificlanguage(DSL)enablesustospecify the annotations and calls the annotation processor (step 3).
guestlanguagesemanticsinamorecompactform.TheDSL Theannotationprocessoriteratesoverallofourannotations
is integrated into the host language, i.e., it is an internal (step 4) and generates Java code for the specialized nodes
DSLwithoutaseparatesyntax.Inourimplementationwith (step5).TheannotationprocessornotifiestheJavacompiler
Java as the host language, the DSL is expressed as Java about the newly generated code, so it is compiled as well
annotations for classes and methods. Java annotations are (step6).Theresultistheexecutablecodethatcombinesthe
an elegant way to attach metadata to Java code that can manuallywrittenandtheautomaticallygeneratedJavacode
| be  | processed | both | during | compilation | and | at run | time. We | (step7). |     |     |
| --- | --------- | ---- | ------ | ----------- | --- | ------ | -------- | -------- | --- | --- |

Java Annotations @Specialization(rewriteOn=ArithmeticException.class)
| (DSL Definition) |      |     |     |     |     |     | int addInt(int |                      | a, int | b) { |     |     |     |
| ---------------- | ---- | --- | --- | --- | --- | --- | -------------- | -------------------- | ------ | ---- | --- | --- | --- |
|                  |      |     |     |     |     |     | return         | Math.addExact(a, b); |        |      |     |     |     |
| 1                | uses |     |     |     |     |     | }              |                      |        |      |     |     |     |
iterates
4
| Java Code |     |     | annotations |                           |     |     |                 |     |     |     |     |     |     |
| --------- | --- | --- | ----------- | ------------------------- | --- | --- | --------------- | --- | --- | --- | --- | --- | --- |
|           |     |     |             | Java Annotation Processor |     |     | @Specialization |     |     |     |     |     |     |
with Node Specifications
|     |     |     |     |     | (DSL Implementation) |     | double | addDouble(double |     | a, double |     | b) { |     |
| --- | --- | --- | --- | --- | -------------------- | --- | ------ | ---------------- | --- | --------- | --- | ---- | --- |
|     |     |     |     |     |                      |     | return | a + b;           |     |           |     |      |     |
3 calls
| 2   | compiles |     |     |     |             |     | }   |     |     |     |     |     |     |
| --- | -------- | --- | --- | --- | ----------- | --- | --- | --- | --- | --- | --- | --- | --- |
|     |          |     |     |     | 5 generates |     |     |     |     |     |     |     |     |
Java compiler
|     |     |     |     | Generated Java Code for  |     |     | @Generic |     |     |     |     |     |     |
| --- | --- | --- | --- | ------------------------ | --- | --- | -------- | --- | --- | --- | --- | --- | --- |
(javac, Eclipse, …)
|     |     |     |     |     | Specialized Nodes |     | Object addGeneric(Frame f, Object a, Object b) { |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | ----------------- | --- | ------------------------------------------------ | --- | --- | --- | --- | --- | --- |
6 compiles
// Handling of String omitted for simplicity.
| 7   | generates |     |     |     |     |     |     |     |     |     |     |     |     |
| --- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
Number aNum = Runtime.toNumber(f, a);
Number bNum = Runtime.toNumber(f, b);
Executable
|     |     |     |     |     |     |     | return | Double.valueOf(aNum.doubleValue() + |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------ | ----------------------------------- | --- | --- | --- | --- | --- |
bNum.doubleValue());
| Figure | 9. Development |     | and | build | process | for the | }   |     |     |     |     |     |     |
| ------ | -------------- | --- | --- | ----- | ------- | ------- | --- | --- | --- | --- | --- | --- | --- |
annotation-basedDSL. Figure 10. Type specializations defined using the
annotation-basedDSL.
Figure10presentshowtheAddnodespecializationsare ways to reduce redundancy in guest language implementa-
tions.
| expressed         | by using | annotations. |            | Each method | annotated            | by  |             |                |             |            |                  |      |               |
| ----------------- | -------- | ------------ | ---------- | ----------- | -------------------- | --- | ----------- | -------------- | ----------- | ---------- | ---------------- | ---- | ------------- |
|                   |          |              |            |             |                      |     | Our         | code generator |             | is tightly | integrated       | with | the source    |
| a @Specialization |          | annotation   |            | represents  | one specializa-      |     |             |                |             |            |                  |      |               |
|                   |          |              |            |             |                      |     | compiler    | and            | development |            | environment      | for  | the host lan- |
| tion. The         | method   | addInt       | implements |             | the IAdd specializa- |     |             |                |             |            |                  |      |               |
|                   |          |              |            |             |                      |     | guage. This | enables        | using       | the        | same development |      | tools and     |
| tion behavior     | and      | analogously  | addDouble  |             | implements           | the |             |                |             |            |                  |      |               |
DAddspecializationbehavior.The@Genericannotationin- integrated development environment (e.g., Eclipse or Net-
|             |         |                |     |     |                |     | Beans) | to browse | the | generated | code. | The generated | Java |
| ----------- | ------- | -------------- | --- | --- | -------------- | --- | ------ | --------- | --- | --------- | ----- | ------------- | ---- |
| dicates the | generic | implementation |     | of  | the operation. | Ow- |        |           |     |           |       |               |      |
codeisalsoavailableduringdebugging.
ingtothecomplicatedlanguagesemanticsofJavaScript,the
| conversion | of arbitrary |     | values to | numbers | can even | trigger |     |     |     |     |     |     |     |
| ---------- | ------------ | --- | --------- | ------- | -------- | ------- | --- | --- | --- | --- | --- | --- | --- |
4. HighPerformance
theexecutionofuser-definedconversionmethods,i.e.,addi-
tionalJavaScriptmethods. The main overhead in our interpreter comes from dynamic
One node class is generated for each annotated method. dispatch between nodes. However, the targets of those dy-
The generated implementation follows the type transition namicdispatchesareconstantexceptwhenrewritingoccurs.
orderasindicatedinFigure2.Anuninitializedversionofthe We count the number of invocations of a tree and reset the
node is created by the annotation processor without further counterintheeventofanodereplacement.Whenthenum-
definition. We omit the handling of the String type for ber of invocations on a stable tree exceeds a threshold, we
simplicity. speculate that the tree has reached its final state. We then
Method signatures can be used to infer which types are start a special compilation for a specific tree where we as-
expected for the left and right child nodes. Calls to the sumeeverynodeinthetreeremainsunmodified.Thisway,
typed execute methods are generated depending on that in- thevirtualdispatchbetweentheexecutenodescanbecon-
ference. If an UnexpectedResult exception is thrown by verted to a direct call, because the receiver is a constant.
thetypedexecutemethods,thenoderewritesitselftoamore Thesedirectcallsareallinlined,formingonecombinedunit
genericstate.Theannotationprocessorknowsfromthespe- ofcompilationforawholetree.
cializationsignatureshowgenericaspecializationis.There- Becauseeverynodeofthetreeisassumedconstant,many
fore it can order them to achieve the desired specialization values in the tree can also be treated as constants. This
transitions. If a specialization signature is ambiguous, this helpsthecompilerproduceefficientcodefornodesthathave
order can also be defined manually. In the definition of the constant parameters. Examples include the actual value of
specializations,thetransitioncanbetriggeredbyexceptions a constant node, the index of a local variable access (see
as well as by explicitly specified guards. The latter is de- Section 4.2), and the target of a direct call. This special
fined as a method that returns false if the node should be compilationoftheinterpreterbyassumingthenodesinthe
replaced. treetobeconstantsisanapplicationofpartialevaluationto
Theadditionexamplehighlightshowwecandefinespe- generatecompiledcodefromaninterpreterdefinition[21].
cializations for binary nodes using metadata. But this ap- Everycontrolflowpathleadingtoanodereplacementis
proachisnotlimitedtothedefinitionofbinarynodes.Itcan replaced with a deoptimization point. Those points invali-
beusedtogenerateanykindofspecializingnodeswithany date the compiled code and continue executing the tree in
numberofchildnodes.Weusethisapproachinanumberof theinterpretermode.Arewriteperformedimmediatelyafter

usenodereplacementinordertospecializeonthetypeofa
if (x) {
code block localvariable.Thisallowsfordynamicprofilingofvariable
| } else { |     |     |     |     |     |     | typeswhileexecutingintheinterpreter. |     |     |     |     |     |     |     |
| -------- | --- | --- | --- | --- | --- | --- | ------------------------------------ | --- | --- | --- | --- | --- | --- | --- |
rewrite node
|     |     |     |     |     |     |     | The | performance |     | of local | variable | access | is  | critical for |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------- | --- | -------- | -------- | ------ | --- | ------------ |
}
manyguestlanguages.Therefore,itisessentialthatalocal
Figure11. Injectingstaticinformationbynoderewriting. variableaccessinthecompiledcodeafterpartialevaluation
isfast.Weensurethisbyforcinganescapeanalysis(seefor
example[36])ofthearraycontainingthevaluesoflocalvari-
| deoptimization |            | resets | the invocation | counter    | of          | the tree, so |             |            |     |       |        |        |       |             |
| -------------- | ---------- | ------ | -------------- | ---------- | ----------- | ------------ | ----------- | ---------- | --- | ----- | ------ | ------ | ----- | ----------- |
|                |            |        |                |            |             |              | ables. This | eliminates |     | every | access | to the | array | and instead |
| that it is     | recompiled | only   | after          | additional | invocations | cross        |             |            |     |       |        |        |       |             |
connectsthereadofthevariablewiththelastwrite.Thisim-
thecompilationthresholdagain.
|            |            |         |              |                |                |          | plicitly      | creates   | a static       | single | assignment |           | (SSA)     | form [16] |
| ---------- | ---------- | ------- | ------------ | -------------- | -------------- | -------- | ------------- | --------- | -------------- | ------ | ---------- | --------- | --------- | --------- |
| The result | after      | partial | evaluation   |                | is represented | in the   |               |           |                |        |            |           |           |           |
|            |            |         |              |                |                |          | for the local | variables |                | of the | guest      | language. | After     | conver-   |
| compiler’s | high-level |         | intermediate | representation |                | (IR). It |               |           |                |        |            |           |           |           |
|            |            |         |              |                |                |          | sion to       | SSA form, | guest-language |        |            | local     | variables | have no   |
embodiesthecombinedsemanticsofallcurrentnodesinthe
|               |            |            |           |         |               |           | performance    | disadvantage |                |          | compared | to   | host          | language lo- |
| ------------- | ---------- | ---------- | --------- | ------- | ------------- | --------- | -------------- | ------------ | -------------- | -------- | -------- | ---- | ------------- | ------------ |
| tree. This    | IR is      | then given | for       | further | optimizations | to the    |                |              |                |          |          |      |               |              |
|               |            |            |           |         |               |           | cal variables. |              | In particular, |          | the SSA  | form | allows        | the host     |
| host language | optimizing |            | compiler. | The     | compiler      | then per- |                |              |                |          |          |      |               |              |
|               |            |            |           |         |               |           | compiler       | to perform   |                | standard | compiler |      | optimizations | such         |
formsadditionalinlining,andinparticularalsoglobalopti-
asconstantfoldingorglobalvaluenumberingforlocalvari-
mizationsofthewholeIRsuchassharingofcommonoper-
|                |     |       |           |      |         |              | able expressions |     | without | a   | data flow | analysis | for | the frame |
| -------------- | --- | ----- | --------- | ---- | ------- | ------------ | ---------------- | --- | ------- | --- | --------- | -------- | --- | --------- |
| ations between |     | nodes | or global | code | motion. | The compiler |                  |     |         |     |           |          |     |           |
array.Theactualframearrayisneverallocatedinthecom-
canperformmoreglobaloptimizationsthannoderewriting,
piledcode,butonlyduringdeoptimization.
sincenoderewritesarealwayslocaltoonenode.Thismeans
|     |     |     |     |     |     |     | In essence, |     | this optimization |     | for | local | variables | is just a |
| --- | --- | --- | --- | --- | --- | --- | ----------- | --- | ----------------- | --- | --- | ----- | --------- | --------- |
thatarewritewouldonlyoptimizeanodebasedonthelocal
|     |     |     |     |     |     |     | targeted | and well-defined |     | application |     | of  | escape | analysis to |
| --- | --- | --- | --- | --- | --- | --- | -------- | ---------------- | --- | ----------- | --- | --- | ------ | ----------- |
informationavailableatthespecificnode.
thecompilerIRresultingfrompartialevaluation.Thisguar-
Thecompilationprocessneednotbeginattherootnode.
|             |     |         |          |            |       |            | antees predictable |     | high | performance |     | for | guest-language | lo- |
| ----------- | --- | ------- | -------- | ---------- | ----- | ---------- | ------------------ | --- | ---- | ----------- | --- | --- | -------------- | --- |
| It can also | be  | started | only for | a subtree, | i.e., | we compile |                    |     |      |             |     |     |                |     |
calvariables.Useoftheseframefacilitiesforlocalvariables
often-executedloopstructuresbeforetheircontainingmeth-
|          |               |     |         |               |     |               | is optional; | guest-language |     |     | implementations |     | are | not forced |
| -------- | ------------- | --- | ------- | ------------- | --- | ------------- | ------------ | -------------- | --- | --- | --------------- | --- | --- | ---------- |
| ods. For | this purpose, |     | we have | an invocation |     | counter for a |              |                |     |     |                 |     |     |            |
tousethem.
| specific subtree |     | that is | also reset | in  | case of | node replace- |                         |     |         |          |     |          |               |     |
| ---------------- | --- | ------- | ---------- | --- | ------- | ------------- | ----------------------- | --- | ------- | -------- | --- | -------- | ------------- | --- |
| ment.            |     |         |            |     |         |               | 4.3 BranchProbabilities |     |         |          |     |          |               |     |
|                  |     |         |            |     |         |               | An optimizing           |     | dynamic | compiler |     | uses the | probabilities | of  |
4.1 InjectingStaticInformation
thetargetsofabranchtoproducebetterperformingmachine
Speculative optimization allows a node to inject additional code. The execution frequency of a certain block controls
| static information |     | into | the compilation |     | process, | yet safely |               |     |         |        |          |     |         |              |
| ------------------ | --- | ---- | --------------- | --- | -------- | ---------- | ------------- | --- | ------- | ------ | -------- | --- | ------- | ------------ |
|                    |     |      |                 |     |          |            | optimizations |     | such as | method | inlining |     | or tail | duplication. |
fallbacktodeoptimizedcodeintherarecasewhenthisin- Additionally, an optimized code layout can decrease the
formation is falsified. Figure 11 illustrates the prototypical numberofbranchorinstructioncachemisses.
formofsuchanode.Whenthisnodeisinterpreted,thecon-
Branchprobabilityinformationfromexecutingthebase-
ditionxisdynamicallycheckedoneverynodeinvocation.In lineexecutionofthehostsystemisavailableinthecompiler
| the case | where | x is true, | additional | compiler |     | optimizations |          |         |             |     |          |     |              |       |
| -------- | ----- | ---------- | ---------- | -------- | --- | ------------- | -------- | ------- | ----------- | --- | -------- | --- | ------------ | ----- |
|          |       |            |            |          |     |               | IR after | partial | evaluation. |     | However, | for | a particular | node, |
(insidetheif-branch)canbeapplied,yieldingafaster(but thisinformationistheaverageofallexecutionsofthatkind
specialized) implementation of the node’s operation. When of node. This can differ significantly from the information
thenodeiscompiled,theelseblockwiththenoderewrite
|     |     |     |     |     |     |     | that would | have | been | gathered | for | the particular |     | node the |
| --- | --- | --- | --- | --- | --- | --- | ---------- | ---- | ---- | -------- | --- | -------------- | --- | -------- |
isconvertedintoadeoptimizationpoint.Thismeansthatfor partial evaluator is currently adding to the combined com-
| anycodethatfollowsfromthisnode,thecompilerassumesx |     |     |     |     |     |     | pilerIR. |     |     |     |     |     |     |     |
| -------------------------------------------------- | --- | --- | --- | --- | --- | --- | -------- | --- | --- | --- | --- | --- | --- | --- |
tobetrue.Thecompileronlyinsertsaconditionaltriggerof
Figure12showsanexampleASTnodethatimplements
deoptimizationincasethevalueofxisfalse.Thedynamic the semantics of a conditional operation of the guest lan-
checkonxintheinterpreteristransformedintostaticinfor-
|     |     |     |     |     |     |     | guage. | Depending | on  | the | evaluation | of  | condition, | ei- |
| --- | --- | --- | --- | --- | --- | --- | ------ | --------- | --- | --- | ---------- | --- | ---------- | --- |
mationprecededbyadeoptimizationpointinthecompiler. ther the value obtained by executing the thenPart or the
Ifxisanexpressioninsteadofasimplebooleanvariable,the elsePart node is returned. This is implemented using an
| compiler | can still | extract | and | use static | information, | e.g., a |     |     |     |     |     |     |     |     |
| -------- | --------- | ------- | --- | ---------- | ------------ | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
ifstatementofourhostlanguage.
limitedvaluerangeofanintegervariable. Thebranchprobabilityforthisifwouldbetheaverageof
allguestlanguageconditionalnodes,becausetheyallshare
4.2 LocalVariables
|     |     |     |     |     |     |     | the same | code | and profiling |     | is usually | method |     | based in the |
| --- | --- | --- | --- | --- | --- | --- | -------- | ---- | ------------- | --- | ---------- | ------ | --- | ------------ |
Reading and writing local variables is performed by guest hostsystem.Asimilarproblemariseswhenguestlanguage
languages via an index into a Frame object that contains a loop constructs are implemented using host language loop
framearrayholdingthevalues.Localvariableaccessnodes constructs.Thehostsystemfacilitiesforloopfrequencypro-

| Object execute() { |     |     |     | Object execute() { |     |     |     |     |     |
| ------------------ | --- | --- | --- | ------------------ | --- | --- | --- | --- | --- |
if (condition.execute() == Boolean.TRUE) { if (injectBranchProbability(
return thenPart.execute(); thenCounter / (thenCounter + elseCounter),
| } else { |                     |     |     |                | condition.execute() == Boolean.TRUE)) { |     |     |     |     |
| -------- | ------------------- | --- | --- | -------------- | --------------------------------------- | --- | --- | --- | --- |
| return   | elsePart.execute(); |     |     |                |                                         |     |     |     |     |
| }        |                     |     |     | if             | (inInterpreter()) {                     |     |     |     |     |
| }        |                     |     |     | thenCounter++; |                                         |     |     |     |     |
}
Figure12. Exampleofaconditionalnodethatneedsguest- return thenPart.execute();
| levelbranchprobabilities. |     |     |     | } else | {   |     |     |     |     |
| ------------------------- | --- | --- | --- | ------ | --- | --- | --- | --- | --- |
if (inInterpreter()) {
elseCounter++;
| filingarenolongersufficientastheywouldalwaysproduce |     |     |     | }   |     |     |     |     |     |
| --------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
the average of all loops of a specific guest language opera- return elsePart.execute();
}
tion.
}
| Therefore, | our system | supports the injection | of branch |           |                                          |     |     |     |     |
| ---------- | ---------- | ---------------------- | --------- | --------- | ---------------------------------------- | --- | --- | --- | --- |
|            |            |                        |           | Figure13. | Exampleconditionalnodeextendedwithguest- |     |     |     |     |
probabilitiesandloopfrequenciesbytheguestlanguagein-
terpreter.Thosevaluesoverwritetheexistingvaluesderived levelbranchprobabilities.
fromthehostsystemduringpartialevaluation.Thecodefor
measuring the values is only present in the interpreter and ensurethatthecheckofthestablevariablehasnooverhead
removed in the compiled code. This is an example where in compiled code and the system still executes correctly in
theinterpreterhastogobeyondonlyimplementingthelan-
allcases.
guagesemantics,butdoadditionalprofilingfortheoptimiz-
ingcompiler.However,thisadditionalprofilingisoptional, 4.5 FlexibleRuntimeCallInlining
and the interface for providing the probability to the opti- Afterinliningtheexecutemethodsofnodesandtheescape
mizingcompilerislanguageagnostic. analysis of the frame, there can still be calls remaining in
Figure13showstheexampleconditionalnodeextended
thecompilerIR.Inparticular,forimplementingmorecom-
withguest-levelbranchprobabilities.Itinvokesstaticmeth- plexsemantics,anodecancallhelpermethodsfromitsown
ods that are compiler directives, i.e., they behave differ- methods. Those calls are equivalent to runtime calls in tra-
ently in the interpreter and in compiled code. The method ditional dynamic compilation systems. One specific advan-
inInterpreterreturnsalwaystrueintheinterpreter,but tageofoursystemisthatthecodebehindthoseruntimecalls
| always false | in compiled | code. Therefore, | the counter |            |        |          |              |                 |     |
| ------------ | ----------- | ---------------- | ----------- | ---------- | ------ | -------- | ------------ | --------------- | --- |
|              |             |                  |             | is neither | native | code nor | hand-written | assembler code, | but |
increments are only performed during interpretation. The again just code written in the host language. The host lan-
methodinjectBranchProbabilityisano-opintheinter- guageoptimizingcompilercaninlinethosecalls.
preter, but attaches the provided probability to the if-node Whether to inline such a runtime call can be decided
inthecompiler. individually for every operation in the system. This allows
|     |     |     |     | us to use | probability | information | to  | guide that decision. | A   |
| --- | --- | --- | --- | --------- | ----------- | ----------- | --- | -------------------- | --- |
4.4 Assumptions
|     |     |     |     | runtime | call on a | particularly | hot path | is more likely | to be |
| --- | --- | --- | --- | ------- | --------- | ------------ | -------- | -------------- | ----- |
Some guest languages need to register global assumptions inlined than a runtime call on a cold path. The decision of
about the system state in order to execute efficiently. Ex- how much of the code of an operation is inlined into the
amples of such assumptions are: the current state of a Java currentcompilationishighlyflexible.
| class hierarchy, | and the | redefinition of well-known | system |     |     |     |     |     |     |
| ---------------- | ------- | -------------------------- | ------ | --- | --- | --- | --- | --- | --- |
4.6 CompilationExample
| library objects | for JavaScript. | Traditionally, | every VM has |     |     |     |     |     |     |
| --------------- | --------------- | -------------- | ------------ | --- | --- | --- | --- | --- | --- |
a language-specific system of registering such dependen- ThissectioncontinuestheexampleofSection3.4andshows
ciesofthecompiledcode.Oursystemprovidesalanguage- howthemethodiscompiledtooptimizedmachinecode.Re-
agnosticwayofcommunicatingassumptionstotheruntime callthatweshowedtheASTofasimpleJavaScriptmethod
| system. |     |     |     | (seeFigure4)attwostablestates:nodesspecializedtotype |     |     |     |     |     |
| ------- | --- | --- | --- | ---------------------------------------------------- | --- | --- | --- | --- | --- |
Aguestlanguageinterpretercanrequestaglobalvariable integer(seeFigure7)anddouble(seeFigure8).
object from the runtime that encapsulates a stable boolean Assumethattheexamplemethodiscalledfrequentlywith
value.Theinitialvalueistrueandthevariablecanonlybe asmallvalueforargumentn,i.e.,theASTspecializedtoin-
modifiedasingletimetobefalse.Intheinterpreter,anode tegerisexecutedoftenwithoutbeingrewritten.Eventhough
canchecksuchavariabledynamicallyforitsvalue.During the interpreter is optimized and specialized, the interpreter
partialevaluation,weassumethevalueofthevariabletobe dispatch overhead remains. It is not possible to reach ex-
stable.Wereplacetheaccesstothevariablewiththecurrent cellentpeakperformancewithoutcompilation.Partialeval-
constantandregisteradependencyofthecompiledcodeon uation eliminates the interpretation overhead, resulting in
the value. If subsequently the value changes, any compiled the Intel x86 machine code shown in Figure 14. The figure
code relying on the value is deoptimized. This way, we showsallofthemachinecodefortheloop,butomitsthepro-

access argument 0 (JavaScript variable n) access argument 0 (JavaScript variable n)
type check that n is of type Integer type check that n is of type Integer
| deoptimize if check fails |     |     |     | deoptimize if check fails |     |     |     |     |     |     |
| ------------------------- | --- | --- | --- | ------------------------- | --- | --- | --- | --- | --- | --- |
| unbox n into register esi |     |     |     | unbox n into register esi |     |     |     |     |     |     |
mov       eax, 1     // JavaScript variable i mov       eax, 1     // JavaScript variable i
mov       ebx, 0     // JavaScript variable sum xorpd     xmm0, xmm0 // JavaScript variable sum
| jmp       L2           |     |     |     | jmp       L2            |     |     |     |     |     |     |
| ---------------------- | --- | --- | --- | ----------------------- | --- | --- | --- | --- | --- | --- |
| L1: mov       edx, ebx |     |     |     | L1: cvtsi2sdl xmm1, eax |     |     |     |     |     |     |
add       edx, eax   // Writes the overflow flag addsd     xmm0, xmm1
| jo        L3 | // Jump if overflow flag is true |     |     | incl      eax |     |     |     |     |     |     |
| ------------ | -------------------------------- | --- | --- | ------------- | --- | --- | --- | --- | --- | --- |
incl      eax safepoint            // Host(cid:350)specific yield code
safepoint            // Host(cid:350)specific yield code L2: cmp       eax, esi
| mov       ebx, edx                |     |     |     | jlt       L1     |                                        |     |     |     |     |     |
| --------------------------------- | --- | --- | --- | ---------------- | -------------------------------------- | --- | --- | --- | --- | --- |
| L2: cmp       eax, esi            |     |     |     | box xmm0         | (sum) into Double object               |     |     |     |     |     |
| jlt       L1                      |     |     |     | return boxed sum |                                        |     |     |     |     |     |
| box ebx (sum) into Integer object |     |     |     |                  |                                        |     |     |     |     |     |
|                                   |     |     |     | Figure16.        | Examplemachinecodespecializedtodouble. |     |     |     |     |     |
return boxed sum
L3: call      deoptimize
| Figure14. Examplemachinecodespecializedtointeger. |           |     |     |                 |                |     |             |              |               |       |
| ------------------------------------------------- | --------- | --- | --- | --------------- | -------------- | --- | ----------- | ------------ | ------------- | ----- |
|                                                   |           |     |     | if the addition | overflows.     |     | Therefore,  | we need      | a conditional |       |
|                                                   |           |     |     | jump after      | the addition;  | if  | an overflow | happens,     | we            | deop- |
|                                                   |           |     |     | timize.         | Deoptimization | is  | a call      | to a runtime | function,     | i.e., |
| ForNode.execute                                   | [bci: 39] |     |     |                 |                |     |             |              |               |       |
local 0 (this) = const ForNode@1005245720 thereisnoneedtoemitanymoremachinecodethanasingle
local 1 (frame) = vobject 0 callinstruction.Metadataassociatedwiththecallisusedto
| BlockNode.execute | [bci: 20] |     |     |     |     |     |     |     |     |     |
| ----------------- | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
local 0 (this) = const BlockNode@169916747 restore the AST interpreter stack frames, as described later
local 1 (frame) = vobject 0 inthissection.Theincrementoftheloopvariableiisalso
local 2 (i) = const 3 specializedto integer,butdoesnot needan overflowcheck
| FunctionNode.execute | [bci: 5] |     |     |           |           |       |       |           |       |         |
| -------------------- | -------- | --- | --- | --------- | --------- | ----- | ----- | --------- | ----- | ------- |
|                      |          |     |     | since the | lower and | upper | bound | are known | to be | integer |
local 0 (this) = dead
values.Aftertheloop,theaccumulatedsumisconvertedto
local 1 (frame) = dead
OptimizedCallTarget.executeHelper [bci: 15] aboxedIntegerobjectandreturned.
local 0 (this) = dead Themachinecodefortheloopisshortanddoesnotcon-
local 1 (arguments) = dead tain any type checks, object allocations, or pointer tagging
local 2 (frame) = dead
operations.Someofthemovinstructionsmayseemsuperflu-
ousatthefirstglance,butexisttoallowfordeoptimization.
vobject 0 Frame
arguments = r8
|     |     |     |     | Since deoptimization |     | needs | to restore | the | AST interpreter |     |
| --- | --- | --- | --- | -------------------- | --- | ----- | ---------- | --- | --------------- | --- |
primitiveLocals = vobject 1
|     |     |     |     | stack frames | with | values | before | the overflowing | operation, |     |
| --- | --- | --- | --- | ------------ | ---- | ------ | ------ | --------------- | ---------- | --- |
objectLocals = vobject 2
theinputvaluesoftheadditionneedtobekeptaliveforpo-
vobject 1 long[]
0 = ebx   // JavaScript variable i tentialuseduringdeoptimization.
1 = eax   // JavaScript variable sum Figure 15 shows the metadata recorded for deoptimiza-
2 = esi   // JavaScript variable n
|     |     |     |     | tion. It | is associated | with | the program | counter | of  | the call |
| --- | --- | --- | --- | -------- | ------------- | ---- | ----------- | ------- | --- | -------- |
vobject 2 Object[]
|     |     |     |     | instruction, | i.e., a | metadata | table | lookup with | this | program |
| --- | --- | --- | --- | ------------ | ------- | -------- | ----- | ----------- | ---- | ------- |
0 = const null
counterreturnsthedeoptimizationinformationserializedin
1 = const null
2 = const null a compressed form (see for example [37, 54]). Four inter-
|                           |             |                |     | preter stack | frames | need to | be restored: | the | helper | method |
| ------------------------- | ----------- | -------------- | --- | ------------ | ------ | ------- | ------------ | --- | ------ | ------ |
| Figure 15. Deoptimization | information | to restore AST | in- |              |        |         |              |     |        |        |
wherecompilationwasstartedandthatisresponsibleforal-
terpreterframesandallocateescape-analyzedobjects.
locatingtheASTinterpreterstackframe;andthreeexecute
|     |     |     |     | methods | of the AST. | Even | though | deoptimization |     | happens |
| --- | --- | --- | --- | ------- | ----------- | ---- | ------ | -------------- | --- | ------- |
logue and epilogue of the loop. The prologue accesses the nested more deeply during the execution of the addition
firstmethodargument,whichispassedinasaboxedobject. node,itissufficienttorestartinterpretationatthebeginning
It checks that the argument is of the box type Integer; if ofthecurrentloopiteration.Thiskeepsthemetadatasmall.
not,itdeoptimizestotheASTinterpreterinordertochange TheASTinterpreteroperatesonaheap-allocatedFrame
thetypespecialization.Theunboxedprimitiveintegervalue object. For improved performance, we apply escape analy-
isstoredinregisteresi. sisofthisobjectduringcompilation(seeSection4.2).This
The loop performs an addition to accumulate the sum Frameobject,togetherwiththeactualdataarraysthatstore
variable. It is specialized to integer, so the normal integer thevalueoflocalvariables,needtobeallocatedduringde-
addinstructionisused.However,thenormalbehaviorofthe optimization. The necessary information for this allocation
Intel instruction set is to wrap around to negative numbers isstoredinthevobjectsectionsofthemetadata.

Guest Language Application
Guest Language Implementation Language Parser AST Interpreter
Truffle API Framework for Node Rewriting
Truffle Optimizer Partial Evaluation using Graal
Hosted on any Java VM
VM Runtime Services Garbage Collector Graal Compiler (slow, for guest language
development and debugging only)
Stack Walking Deoptimization
AOT Optimization: using Graal for static analysis and AOT compilation Hosted on Graal VM
(fast, for integration of guest language
OS code with existing Java applications)
Figure17. DetailedsystemstructureofourJava-basedprototype,withadditionaldeploymentstrategiesonanyJavaVMand
ourGraalVM.
TheASTnodes,i.e.,thereceiverobjectsoftheexecute 5. ImplementationandDeployment
methods, are provided as literal constants in the metadata.
Our prototype implementation of guest languages and the
Since the machine code does not access the nodes at all,
hostservicesiswritteninasubsetofJava.Toallowahead-
it is not necessary to keep them in a register. The values
of-time(AOT)compilation,wedonotuseJavafeaturessuch
of local variables in execute methods that are no longer
as reflection and dynamic class loading that would prevent
accessiblebytheASTinterpreterneednotberestoredduring
a whole-program static analysis determining the methods
deoptimization.Thisisrepresentedasdeadinthefigure.
and fields in use. Since we use Java as a systems program-
Note that all primitive JavaScript local variables are
minglanguage[19],omittingthesefeaturesisnottoorestric-
stored in a long[] array (the vobject 1 in the figure),
tive. AOT compilation is performed by the Graal compiler,
regardlessoftheiractualtype(integerordoubleinthecase
i.e., we use the same compiler for ahead-of-time compila-
of JavaScript). The AST nodes that access the frame ob-
tionthatwealsousefordynamiccompilation.Thisprocess
ject are all typed, i.e., all reads and writes are specialized
produces a runtime environment that is able to run the lan-
to the same type. This ensures that the raw bits stored into
guages for which an implementation was provided during
thelong[]arrayarealwaysinterpretedasthecorrecttype.
AOT compilation. This is similar to the bootstrapping pro-
Referencescannotbemixedwithprimitivedatabecausethe
cessofmetacircularVMssuchasMaxine[69]orJikes[2].
garbage collector needs to distinguish them, so we have a
However,wewanttonotethatoursystemisnotametacir-
separateObject[]array,withthesamelengthandindexed
cular Java VM, since we are not able to load and execute
withthesamelocalvariablenumbers.
newJavabytecodeatruntime.Weareonlyabletoloadand
Should the example optimized machine code be called
executeguestlanguagesourcecode,asdefinedbytheguest
with a high value for n, the add instruction sets the pro-
languageimplementation.
cessor’s overflow flag to true. The succeeding conditional
Figure 17 shows a refinement of the system structure
jumptriggersdeoptimization,whichinvalidatesthemachine
presented in Figure 1. The host services are split up into
codesothatitisnolongerenteredonsubsequentexecutions
threelayers:
of this method. Execution of the AST continues in the in-
terpreter,whichreplacestheIAddnodewithaDAdd.Later, • Truffle API: This is the API to implement the AST in-
when this version of the AST is considered stable, a new
terpreter of guest languages. It provides the handling of
compilationoftheguestlanguagemethodwiththenewAST
guest language frames and local variables, as well as
istriggered.
means for nodes to rewrite themselves. This is the only
Figure 16 shows the machine code for the method type-
publicAPIthataguestlanguageimplementationneeds;
specialized to double. In contrast to Figure 14, it no longer
thelowerlevelsareinvisibletotheguestlanguageimple-
containsanintegeradditionthatcanoverflow.Theaddsdin-
mentation.
structionthatperformsthedoubleadditionalwayssucceeds.
• Truffle Optimizer: this includes the partial evaluation,
Theprologuecodebeforetheloopisunchanged.Theparam-
and is implemented on top of the API that the Graal
eternisstillassumedtobeoftypeinteger,sodeoptimization
compiler[45]provides.
canhappenintheprologue.Intheepilogue,aboxedDouble
objectiscreatedtoreturntheresult. • VMRuntimeServices:ThislayerprovidesthebasicVM
services such as garbage collection, exception handling,

and deoptimization. It also includes Graal, our aggres- tions and some fairly comprehensive test suites. Truffle’s
sivelyoptimizingcompilerforJava,writteninJava. flexibility allows our implementation of JavaScript to be
mostlystraightforward:JavaScriptobjectsareimplemented
Thislayeredapproachallowstwoalternativedeployment
asaparticularclassofJavaobjects;allJavaScriptvaluesare
scenariosforguestlanguageimplementations.Inbothcases,
|        |             |              |     |       |           |          |     | representable | as  | subtypes | of Java’s | Object | type; and | most |
| ------ | ----------- | ------------ | --- | ----- | --------- | -------- | --- | ------------- | --- | -------- | --------- | ------ | --------- | ---- |
| no AOT | compilation | is performed |     | since | the guest | language |     |               |     |          |           |        |           |      |
oftheoftypechecksrequiredforbasicJavaScriptoperations
implementationrunsasanapplicationonaJavaVM.
areimplementedusingtheinstanceofoperator.
|     |     |     |     |     |     |     |     | Rewriting | is  | used in | two | ways: type | specialization | of  |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | ------- | --- | ---------- | -------------- | --- |
1. HostedonanyJavaVM:Sinceallofourimplementation
ispureJavacode,theguestlanguageimplementationcan generic operations and inline caching. Our type specializa-
run on any unmodified Java VM. However, such a Java tiontechniqueisdiscussedinSection4.6.Inlinecaching(see
|     |     |     |     |     |     |     |     | Section | 3.1) optimizes |     | operations | that | are frequently | per- |
| --- | --- | --- | --- | --- | --- | --- | --- | ------- | -------------- | --- | ---------- | ---- | -------------- | ---- |
VMdoesnotprovideanAPItoitsdynamiccompiler,so
partial evaluation is not possible and the guest language formedonobjectsofaspecifictype.Eachobject’smemory
layoutisrepresentedasashapeobject,whichissharedbyall
isonlyinterpreted.Still,thisscenarioisusefulfordevel-
opmentanddebuggingoftheguestlanguageimplemen- similarly-shapedobjects.Inlinecachinginvolvesreplacinga
tationsinceitminimizestheamountofframeworkcode memberaccessnodewithaversionthatisspecializedfora
specificshape.Thespecializednodecheckstheactualshape
thatisinuse.Additionally,itcanbeusedfordeployment
onlegacysystemswheretheunderlyingJavaVMcannot againsttheexpectedshapeand,onsuccess,executesthesim-
plifiedaccessforthisshape.Becausecachedshapesareim-
bechanged.
|           |          |     |       |     |                   |     |     | mutable | in these | generated | nodes, | machine | code generated |     |
| --------- | -------- | --- | ----- | --- | ----------------- | --- | --- | ------- | -------- | --------- | ------ | ------- | -------------- | --- |
| 2. Hosted | on Graal | VM: | Graal | VM  | is a modification |     | of  |         |          |           |        |         |                |     |
byGraalishighlyefficient,requiringnodynamiclookups.
| the Java  | HotSpot       | VM             | that        | uses Graal | as      | its dynamic |       |             |           |        |           |                |                |      |
| --------- | ------------- | -------------- | ----------- | ---------- | ------- | ----------- | ----- | ----------- | --------- | ------ | --------- | -------------- | -------------- | ---- |
|           |               |                |             |            |         |             |       | This        | general   | inline | caching   | technique      | is extended    | for  |
| compiler. | Additionally, |                | it provides |            | an API  | to access   | the   |             |           |        |           |                |                |      |
|           |               |                |             |            |         |             |       | JavaScript  | by making | it     | aware     | of prototypes. | In JavaScript, |      |
| compiler, | so            | guest language |             | code       | can run | at full     | speed |             |           |        |           |                |                |      |
|           |               |                |             |            |         |             |       | an object’s | members   | are    | inherited | from           | other objects  | in a |
withpartialevaluation.Thisscenarioisespeciallyuseful
|     |     |     |     |     |     |     |     | chain of | prototypes. | This | system | is derived | from Self | [65], |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ----------- | ---- | ------ | ---------- | --------- | ----- |
whenintegratingguestlanguagecodewithexistingJava
butsimpler,asanobjectmayhaveonlyoneprototype.The
| applications, |     | i.e., to | invoke | guest | language | code | from |          |        |            |      |        |          |          |
| ------------- | --- | -------- | ------ | ----- | -------- | ---- | ---- | -------- | ------ | ---------- | ---- | ------ | -------- | -------- |
|               |     |          |        |       |          |      |      | simplest | way of | supporting | this | system | would be | to cache |
JavausingtheexistingJavascriptingAPI[35].
|     |     |     |     |     |     |     |     | each level | of the | prototype | chain: | A cached | check | of the |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------- | ------ | --------- | ------ | -------- | ----- | ------ |
objectitselfwouldrevealthatamemberisnotthere,andso
6. AWideVarietyofLanguages
moveontoacachedcheckofitsprototype,etc.Weexpedite
Thecrucialquestionthatweneedtoansweris:“Whichlan-
|     |     |     |     |     |     |     |     | this by moving |     | the prototype |     | relationship | into the | shapes |
| --- | --- | --- | --- | --- | --- | --- | --- | -------------- | --- | ------------- | --- | ------------ | -------- | ------ |
guages are amenable to the Truffle approach?” In order to themselves;anytwoobjectswiththesameshapeareassured
| answer | this question, | we  | must | undertake | a variety |     | of im- |     |     |     |     |     |     |     |
| ------ | -------------- | --- | ---- | --------- | --------- | --- | ------ | --- | --- | --- | --- | --- | --- | --- |
tohavethesameprototypes,andsoacacheoveranobject’s
plementations for disparate languages. This approach can- shapeisequivalenttoacacheoveritsentireprototypechain.
notyieldanupperboundofgenerality,butwitheachimple- Trufflealsoprovidesthepossibilityforanoveltechnique
| mentation | the known | lower | bound | is  | raised. | If a new | lan- |             |      |       |            |          |         |      |
| --------- | --------- | ----- | ----- | --- | ------- | -------- | ---- | ----------- | ---- | ----- | ---------- | -------- | ------- | ---- |
|           |           |       |       |     |         |          |      | for dealing | with | eval, | a function | provided | by many | lan- |
guage has a totally new feature or is of a radically differ- guagestoexecutestringsascode.EspeciallyforJavaScript,
entparadigmfromexistinglanguages,thenitmayneednew
itiswidelyusedandoftenpoorlyunderstood[48].Although
compileroptimizationstogetgoodperformance,butforfea- manyJavaScriptimplementationscacheparticularvaluesof
turesclosetothoseinlanguagesalreadysupported,adirect evaluatedstrings,rewritingmayadditionallybeusedtospe-
implementationshouldbeabletoreusetheexistingmachin-
cializeonparticularpatternsofevaluatedstrings.Sincethe
ery.Assuch,weaimtoimplementdiverselanguages,creat- vastmajorityofevalcallsitesencounterstringsofasmall
ingintheprocessabroadtoolkitoftechniques. number of simple patterns, this technique can allow for a
Existinglanguagesfrequentlyfallintoanumberoffam-
highlyefficientimplementationofevalincommoncases.
| ilies, and | as such | share | significant | features. |     | Implementing |     |          |     |     |     |     |     |     |
| ---------- | ------- | ----- | ----------- | --------- | --- | ------------ | --- | -------- | --- | --- | --- | --- | --- | --- |
|            |         |       |             |           |     |              |     | 6.2 Ruby |     |     |     |     |     |     |
theminthesameframeworkallowssignificantpartsoflan-
guage implementations to be shared. Even when for prag- Ruby’s design was strongly influenced by Smalltalk. It
maticreasonstheycannotbeshared,existingtechniquescan shares many of the performance challenges of Smalltalk
informtheimplementationofnewlanguages,expeditingthe (and Self). Almost every operation in Ruby is the result of
implementationprocess.Wediscussinthissectionthepar- a method invocation. Hence, efficient method invocation is
ticularchallengesandfeaturesthatarerequired(and,where key,andthebestkindofinvocationisnoinvocation,i.e.,the
applicable,implemented)foreachoftheselanguages. targetisinlinedintothecaller.Truffle’sinlinecaching,along
|                |     |     |     |     |     |     |     | with optimistic                        |     | movement | of the | shape | checks and | method |
| -------------- | --- | --- | --- | --- | --- | --- | --- | -------------------------------------- | --- | -------- | ------ | ----- | ---------- | ------ |
| 6.1 JavaScript |     |     |     |     |     |     |     | inliningbyGraal,allowsustoachievethis. |     |          |        |       |            |        |
JavaScriptisanexcellentlanguagetouseasatestbed,since In Ruby, any method can be redefined, including basic
there exist several correct, high-performance implementa- arithmeticoperations.Thewaywehandleuncommonpaths

based on global assumptions via deoptimization (see Sec- plementation community. It presents many implementation
tion 4.4) allows us to handle redefinitions by registering a challenges, some of which are unique to the language, and
dependence on the existing definition of a basic method. If manyofwhicharecommontomostoralltechnicalcomput-
the method is redefined, then the compiled code is invali- inglanguages.Itshandlingofwhole-arrayoperations,influ-
datedandexecutionresumesintheinterpreter. encedstronglybythedesignofAPL[32],isonesucharea.
TheRubylanguageincludesafeaturecalledfibers,which ManyoptimizationsweredevisedforAPLinthe1970sand
is equivalent to one-shot continuations or coroutines (stor- 1980s that are largely unknown outside the APL commu-
ing and restoring the current execution context). There are nity. Additionally, this style of computation is amenable to
implementationsoftheseconceptsinJavathatrelyonbyte- large-scale parallelization [60]. The convenient expression
code instrumentation [4], but these incur a significant run- ofparallelismisanindustry-widechallenge,andmuchwork
timeoverhead.InpriorworkwehavedevelopednativeVM hasbeenexpendedinthisareasincetheearlyworkonAPL
support for continuations [62] and coroutines [63] in the optimization. The early APL work also did not anticipate
HotSpotVM,whichcanbeleveragedbyoursystemtopro- the radical change in memory hierarchy of the intervening
videfiberoperationswithO(1)complexity. decades.Combiningthemorerecentworkonparallelization
andmemoryexploitationwiththeearlierarrayoptimization
6.3 Python
workseemslikeafertilecombinationfortechnicalcomput-
Similartootherdynamiclanguages,Pythonprovidesarange ing.
ofintegratedcollectiontypes(lists,sets,arrays,etc.).Itsup- Togaininsightintothisarea,wehaveundertakenanim-
portsmultipleinheritance,andsubclassingevenofprimitive plementationofJ,anAPL-derivedlanguage[29].Although
datatypes.Theshapeconcept,asintroducedforJavaScript, notaswidelyused,Jencapsulatesthearray-processingstyle
fitstherequirementsofaPythonimplementationwell. with minimal additional complication. An implementation
Python allows a method to access the call stack via of J serves as an experimentation lab for array processing
sys. getframe(n) and access to local variables of the techniquesfortechnicalcomputing.
caller frame via sys. getframe(n).f locals. Imple- The R programming language incorporates many of the
menting access to caller frames in the AST interpreter is concepts of array programming, but additionally has an
simple: the frame objects can be chained by passing in the object-oriented and list-processing nature with influences
callerframeinamethodcall.Accessingthecallerframeis from Smalltalk and Scheme. Among the challenges fac-
a feature that we think does not need to be fast, i.e., sup- ing an R implementation are: avoiding unnecessary copy-
ported in a compiled method. However, simply not com- ing of vectors; complex method invocation semantics; re-
piling a method that uses getframe is not enough; it can flection (especially on activations); efficient interfaces to
access frames of compiled methods already on the stack. legacylibrariesinCandFortran77;andpartly-lazy,partly-
Since the frame object is elided by escape analysis during functional semantics [43]. However, long-running compu-
compilation,itdoesnotexistinmemoryandcannotbepart tations in R (and technical computing applications in other
of a frame chain. However, we can use the deoptimization languages)offerthepossibilityofextensiverun-timeanaly-
metadata to reconstruct the frame object. Accessing (and sis and optimization without concern for interactive perfor-
possibly changing) the local variables of a method on the mance.Webelievethatdeepinlininginhotloopsexposethe
call stack requires the following steps: walk the Java stack connectionsneededbythecompilertooptimizethesechal-
tothecorrespondingcompiledmethodactivationframe;re- lenging features. For example, whole-loop analysis can be
constructtheframeobjectusingthedeoptimizationmetadata usedtotransformoperationsequencestoeliminateunneces-
for the compiled method; access and change the local vari- saryarraycopies[67].
able value in the frame object; mark the compiled method Graal is also being used within the context of Open-
as deoptimized, so that the execution continues in the AST JDK Project Sumatra [46], to generate code for GPUs. A
interpreterwhencontrolreturnstothemethodwhoseframe GPU backend for array languages such as J and R, and for
object was accessed. In summary, accessing the frame of array-processing libraries for other languages (e.g., River
a compiled method requires support from the VM runtime Trail[50])offersthepotentialofhigh-performanceparallel
services. However, it does not require more metadata and execution and is something we intend to pursue in the near
infrastructure than deoptimization, i.e., all information is future.
alreadyreadilyavailable.
6.5 FunctionalLanguages
6.4 TechnicalComputing:JandR
Ourapproachmaybesuitedtotheimplementationofpure,
There is no widely accepted definition of “technical com- functionallanguagesbutwehavenotpursuedthisindetail.
puting”, but the one we use is the application of numerical Some of the elements of these languages are present in R,
techniquestobusinessandtechnicalproblemsbydomainex- whichismostlyside-effect-freeatthelevelofmethods,and
perts.ItincludeslanguagessuchasRandJ.Aspointedout ispartlylazy[43].Challengesareintheareaoftailcallsand
byMorandatetal.[43],Rhasbeenunderservedbytheim- continuations. We believe that both can be implemented in

plain Java at the AST interpreter level (using for example terpreter optimized for the target language. VM extensions
exceptions to unwind the stack for tail calls), but that it allowustoefficientlysupportlanguagefeaturessuchastail
is easier and more robust with support from the host VM callsandcontinuations.
services. Both tail calls [57] and continuations [62] have
7.2 Self-OptimizingInterpreters
been prototyped for the Java HotSpot VM, but to the best
ofourknowledgenoeffortsareunderwaytoaddthemtothe Most interpreters use some form of bytecode to represent
Javaspecification. the executed program, mainly because of concerns about
execution speed. New techniques for efficiently executing
7. RelatedWork applications written in dynamic programming languages,
however, increasingly rely on modifications to the internal
7.1 PyPy
representation of the running code. This is possible to a
Bolz and Rigo [6] suggested that VMs for dynamic lan- certaindegreewithbytecodeinterpreters[11,12].However,
guages should be generated from simple interpreters writ- wearguedin[72]thatASTinterpretersallowformuchmore
teninahigh-levelbutlessdynamiclanguage.Theideaisto extensivemodifications.
writethelanguagespecificationasaninterpreterthatserves There have been various techniques devised to improve
astheinputforatranslationtoolchainthatgeneratesacus- interpretation performance while preserving the simplicity,
tom VM and tracing dynamic compiler using partial evalu- directnessandportabilityofaninterpreter.Examplesinclude
ation techniques. The result is either a custom VM imple- the work of Casey et al. [13], Ertl and Gregg [17], Gagnon
mented in C or a layer on top of an object-oriented VM. and Hendren [22], Piumarta and Riccardi [47], Shi et al.
ThePyPyprojectputsthismeta-programmingapproachinto [59],Thibaultetal.[64].However,becauseacompilerana-
practiceandstrivesforthegoalofeasingefficientlanguage lyzesamuchlargerfragmentoftheprogram(onceinlining
implementationbyoptimizingabytecodeinterpreterwritten hasbeenapplied)itcanperformmanyglobaloptimizations
ina high-levellanguage. Theheartof PyPyis theRPython thatarebeyondthereachofinterpreters.
language, an analyzable restricted subset of Python that is
7.3 PartialEvaluation
statically typed using type inference, and can be compiled
to efficient low-level code [3]. PyPy implements a Python Partialevaluationhasalonghistoryandhasbeenextensively
VMwritteninRPython.BoththePythoninterpreterandthe studied for functional languages. Using partial evaluation
RPython VM itself are written in RPython. The RPython to derive compiled code from an interpreter and the source
toolchain translates the interpreter and all necessary sup- codeoftheapplicationwasconceivedbyFutamura[21]and
portcodetoCcode.Italsogeneratesatracingcompiler,for is called the first Futamura projection. Repeating the spe-
whichtheinterpreterauthormustinserthintsinformofrun- cialization process results in a compiler (second Futamura
time calls and annotations [8, 49]. Some of those hints are projection)andatooltogeneratecompilers(thirdFutamura
requiredbycompilergeneratortoidentifyprogramcounter projection).ThetechniqueusedinTrufflecanbeconceived
variables and possible entry and exit points for the tracer. asaspecialformofthefirstFutamuraprojection,wherethe
Otherhintsexposeconstantfoldingopportunitiesandcanbe interpreter is already specialized for the source code, and
usedtoimplementrun-timefeedback[9].Thetracingcom- compiled code is derived from the interpreter only for hot
pilerperformsonlinepartialevaluationontracesasaformof andstablecode.
escapeanalysistoremoveallocations,includingboxedrep- Partial evaluators can be classified as offline and on-
resentationsofprimitivevalues[10].PyPywasexplicitlyde- line[33].Theofflinestrategyperformsbindingtimeanaly-
signedwithmulti-languagesupportinmind.Currently,there sisbeforepartialevaluationthatannotatestheprogramwith
areVMsforPython,Ruby,Converge,andProlog[7];other specializationdirections.Inputsaredeclaredaseitherknown
languageimplementationsareindevelopment. at specialization time (static) or unknown (dynamic). Ide-
BolzandRigo[6]andlaterBolzandTratt[7]statedthat ally,theanalysiswouldalsoguaranteethatthespecialization
ingeneral-purposeobject-orientedVMs,thecompilerisop- alwaysterminates.Automaticpartialevaluatorsusuallyuse
timized for the language, or group of languages, it was de- thisstrategy.Theonlinestrategymakesdecisionsonwhatto
signedfor.Ifalanguage’ssemanticsaresignificantlydiffer- specialize during the specialization process. This approach
entandthusdonotfittheVMwell,itwillperformpoorly— canproducebetterresults[33,53]butisusuallyslowerand
despite a highly optimized underlying VM. Various lan- hasterminationdifficultiesiftheprogramcontainsiterative
guage implementations running atop the Java HotSpot VM or recursive loops [33, 40]. Therefore, online partial eval-
andMicrosoft’s.NETCLRseemtoconfirmthis.Although uators usually rely on programmer-supplied annotations to
theintroductionofinvokedynamic[51]broughtconsiderable directthespecialization.
performance improvements, highly optimized hand-crafted Since we perform online partial evaluation at run time,
VMswritteninalow-levellanguagestillhavetheedgeover wewanttoguaranteethatouralgorithmalwaysterminates.
Java-based runtimes [7]. Our system tries to close the per- Weachievethisbyclearlylimitingthescopeofpartialeval-
formance gap through run-time partial evaluation of an in- uationtotheASTandbyprohibitingrecursionandchanges

totheASTstateinpartiallyevaluatedcode(unlessguarded plementationofmethoddispatchmoreefficient.Challenges
bydeoptimization).Ourmainuseforpartialevaluationisto remainineliminatingtheboxingofnumbers,efficienthan-
eliminatethebiggestoptimizationbarrierinoursystem,the dling of method redefinition of basic operations, and else-
| virtualdispatch. |     |     |     |     |     |     |     | where. |     |     |     |     |     |     |     |
| ---------------- | --- | --- | --- | --- | --- | --- | --- | ------ | --- | --- | --- | --- | --- | --- | --- |
PartialevaluationasameanstospecializeJavaprograms Another approach is to add support for dynamic lan-
has been pursued by several research efforts. Schultz et al. guages to an existing high-performance static-language
| decided to | translate | Java | code | to C | code | that serves | as the | VM[14,31]. |     |     |     |     |     |     |     |
| ---------- | --------- | ---- | ---- | ---- | ---- | ----------- | ------ | ---------- | --- | --- | --- | --- | --- | --- | --- |
inputtoanofflinepartialevaluator.Theresidualcodeisei- A number of projects haveattempted to use LLVM [38]
ther compiled by a C compiler [55] or translated back to as a compiler for high-level managed languages, such as
Javabytecode[56].So-calledspecializationclassesdeclare Rubinius and MacRuby for Ruby [39, 52], Unladen Swal-
optimization opportunities for object-oriented design pat- lowforPython[66],SharkandVMKitforJava[5,23],and
ternssuchasvisitorandstrategy.Theyreportedasignificant McVM for MATLAB [24]. These implementations have to
speedup on selected benchmarks. Masuhara and Yonezawa provideatranslatorfromtheguestlanguages’high-levelse-
[41] proposed automatic run-time bytecode specialization manticstothelow-levelsemanticsofLLVMIR.Incontrast,
foranon-object-orientedsubsetofJavawithanofflinestrat- our approach requires only an AST interpreter; our system
egy.Affeldtetal.[1]extendedthissystemtoincludeobject- canbethoughtofasaHigh-LevelVirtualMachine(HLVM).
orientedfeatureswithafocusoncorrectness.Thespeedups
7.5 Multi-LanguageSystems
| achieved | by this  | system | were        | significant |                 | for | non-object- |          |                 |                |     |             |           |          |        |
| -------- | -------- | ------ | ----------- | ----------- | --------------- | --- | ----------- | -------- | --------------- | -------------- | --- | ----------- | --------- | -------- | ------ |
|          |          |        |             |             |                 |     |             | Wolczko  | et al.          | [70] described |     | an approach |           | for high | per-   |
| oriented | code but | less   | substantial | for         | object-oriented |     | code.       |          |                 |                |     |             |           |          |        |
|          |          |        |             |             |                 |     |             | formance | implementations |                | of  | multiple    | languages |          | (Self, |
ShaliandCook[58]implementedanoffline-styleonlinepar-
tialevaluatorinamodifiedJavacompiler.Theirpartialeval- Smalltalk,Java)atoptheSelfVM.TheSelfVMwaswritten
inC++,andtheguestlanguageswereimplementedinSelf,
| uator derives | residual |     | code | from invariants |     | manually | spec- |     |     |     |     |     |     |     |     |
| ------------- | -------- | --- | ---- | --------------- | --- | -------- | ----- | --- | --- | --- | --- | --- | --- | --- | --- |
ified using source code annotations. It strictly differenti- either by translation to Self source (Smalltalk) or bytecode
atesbetweencompile-timeandrun-timevariables:compile- (Java). This approach relied on the minimality and flexi-
|     |     |     |     |     |     |     |     | bility of | Self, and | the | deep inlining | performed |     | by  | the Self |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --------- | --- | ------------- | --------- | --- | --- | -------- |
timevariablesarecompletelyeliminatedfromresidualcode.
Generated code is as efficient as that of Schultz et al. [56] VM’sinliningcompiler.Asimilarapproachistakenbythe
|                  |     |     |               |           |     |     |            | Java implementation |     | in  | Smalltalk/X | [61]. | The | Virtual | Vir- |
| ---------------- | --- | --- | ------------- | --------- | --- | --- | ---------- | ------------------- | --- | --- | ----------- | ----- | --- | ------- | ---- |
| They illustrated |     | how | their partial | evaluator |     | can | be used to |                     |     |     |             |       |     |         |      |
optimize regular expressions when the pattern is known at tual Machine incorporated an architecture for multi-lingual
| compile | time. While |     | we also | have | compile-time |     | variables | VMs[18]. |     |     |     |     |     |     |     |
| ------- | ----------- | --- | ------- | ---- | ------------ | --- | --------- | -------- | --- | --- | --- | --- | --- | --- | --- |
thatareeliminatedfromthecode,wecanundothisspecial-
7.6 MetacircularVMs
izationatanytimethankstorecordeddeoptimizationinfor-
|     |     |     |     |     |     |     |     | In traditional | VMs, | the | host | (implementation) |     | and | guest |
| --- | --- | --- | --- | --- | --- | --- | --- | -------------- | ---- | --- | ---- | ---------------- | --- | --- | ----- |
mation.Thereforewedonotneedtospecializeallpossible
|     |     |     |     |     |     |     |     | languages | are | unrelated, | and | the host | language | is  | usually |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | ---------- | --- | -------- | -------- | --- | ------- |
branchesthatdependoncompile-timevalues.
Partial evaluatorstargeting Java sufferfrom thefact that lower-levelthantheguestlanguage.Incontrast,metacircular
|               |     |        |       |         |                   |     |       | VMs are | written | in the | guest | language, | which | allows | for |
| ------------- | --- | ------ | ----- | ------- | ----------------- | --- | ----- | ------- | ------- | ------ | ----- | --------- | ----- | ------ | --- |
| Java bytecode |     | cannot | fully | express | all optimizations |     | [55]. |         |         |        |       |           |       |        |     |
sharingofcomponentsbetweenhostandguestsystems.The
| The Truffle         | stack    | gives     | us more | control       | because |           | we do not  |              |                |          |              |                |                |     |        |
| ------------------- | -------- | --------- | ------- | ------------- | ------- | --------- | ---------- | ------------ | -------------- | -------- | ------------ | -------------- | -------------- | --- | ------ |
|                     |          |           |         |               |         |           |            | “boot image” | used           | to start | the          | system         | is constructed |     | ahead  |
| need to re-generate |          | bytecodes |         | for partially |         | evaluated | guest      |              |                |          |              |                |                |     |        |
|                     |          |           |         |               |         |           |            | of time,     | usually        | using    | an alternate | implementation |                |     | of the |
| language            | methods; | instead,  |         | we only       | work    | on        | Graal com- |              |                |          |              |                |                |     |        |
| pilerIR.            |          |           |         |               |         |           |            | language.    |                |          |              |                |                |     |        |
|                     |          |           |         |               |         |           |            | The          | Jikes Research |          | VM [2]       | and            | the Maxine     | VM  | [69]   |
7.4 OtherApproachestoHighPerformance are examples for metacircular Java VMs. Both are focused
Theuseofspeculationanddeoptimizationasaperformance- mainlyattheresearchcommunity.Weusemanyideasfrom
enhancing technique was introduced in Self [27], to allow metacircularVMsforourall-JavaprototypeofVMruntime
|           |              |     |       |       |      |        |          | services. | However, | our | goal is | not a metacircular |     | Java | VM  |
| --------- | ------------ | --- | ----- | ----- | ---- | ------ | -------- | --------- | -------- | --- | ------- | ------------------ | --- | ---- | --- |
| debugging | of optimized |     | code. | Since | then | it has | been ap- |           |          |     |         |                    |     |      |     |
pliedtoarray-boundcheckelimination[71],escapeanalysis sincewearenotabletoloadandexecuteJavaapplications.
Weareonlyabletoloadandexecuteguestlanguagesource
| and stack | allocation | [36], | object | fusing | [68], | boxing | elimi- |     |     |     |     |     |     |     |     |
| --------- | ---------- | ----- | ------ | ------ | ----- | ------ | ------ | --- | --- | --- | --- | --- | --- | --- | --- |
nation[15]andpartialredundancyelimination[44]. code,asdefinedbytheguestlanguageimplementation.
| JRuby | [34] is | an implementation |     |     | of Ruby | that | compiles |     |     |     |     |     |     |     |     |
| ----- | ------- | ----------------- | --- | --- | ------- | ---- | -------- | --- | --- | --- | --- | --- | --- | --- | --- |
8. Conclusions
| to Java bytecode, |     | run | on a Java | VM. | The | challenge | here is |     |     |     |     |     |     |     |     |
| ----------------- | --- | --- | --------- | --- | --- | --------- | ------- | --- | --- | --- | --- | --- | --- | --- | --- |
tomapthesemanticsofadynamically-typedlanguageonto WepresentedanewapproachtoVMconstructionbasedona
astatically-typedinstructionsetandgetgoodperformance. combinationofnoderewritingduringASTinterpretation,an
The JavaVM knows nothing ofthe semantics ofRuby and optimizing compiler, and deoptimization. The compiler ex-
there is no mechanism to communicate optimization infor- ploitsthestructureoftheinterpreter,ineffectpartiallyeval-
mation through Java bytecode. The recent addition of in- uating the interpreter when generating code. Using this ap-
vokedynamictotheJavabytecodeset[51]hasmadetheim- proach we are implementing a variety of languages that to

datehavemostlynothadoptimizingcompilers.Eachimple-
|     |     |     |     |     |     | [3] D. Ancona, | M.  | Ancona, | A.  | Cuni, | and N. | D. Matsakis. |     |
| --- | --- | --- | --- | --- | --- | -------------- | --- | ------- | --- | ----- | ------ | ------------ | --- |
mentation consists of a language-specific AST interpreter; RPython: A step towards reconciling dynamically and stati-
the compiler is reused for all languages. We have adopted cally typed OO languages. In Proceedings of the Dynamic
|     |     |     |     |     |     | LanguagesSymposium,pages53–64.ACMPress,2007. |     |     |     |     |     |     | doi: |
| --- | --- | --- | --- | --- | --- | -------------------------------------------- | --- | --- | --- | --- | --- | --- | ---- |
alayeredimplementationapproach,usingJavaasanimple-
10.1145/1297081.1297091.
mentationlanguage.Ourinterpretersrunfaithfully,butper-
hapswithmodestperformance,onanycompliantJavaVM. [4] Apache Commons. Javaflow, 2009. URL http://commons.
apache.org/sandbox/javaflow/.
WhencombinedwiththeGraalcompiler,weobservesignif-
icant performance improvements. A language implementa- [5] G.Benson. ZeroandShark:azero-assemblyportofOpen-
|                 |              |              |       |           |     | JDK, | 2009. URL | http://today.java.net/pub/a/today/2009/05/ |     |     |     |     |     |
| --------------- | ------------ | ------------ | ----- | --------- | --- | ---- | --------- | ------------------------------------------ | --- | --- | --- | --- | --- |
| tion consisting | of a Truffle | interpreter, | Graal | compiler, | and |      |           |                                            |     |     |     |     |     |
associatedruntimecanalsobecompiledaheadoftimewith 21/zero-and-shark-openjdk-port.html.
Graal to realize a standalone language VM, not requiring a [6] C. F. Bolz and A. Rigo. How to not write virtual machines
JavaVMexceptduringbootstrapping.InsuchaVM,Graal for dynamic languages. In Proceedings of the Workshop on
isalsousedasadynamiccompiler. DynamicLanguagesandApplications,2007.
We achieve high performance from a combination of [7] C.F.BolzandL.Tratt.Theimpactofmeta-tracingonVMde-
techniques: signandimplementation.ScienceofComputerProgramming,
|        |                       |     |             |            |       | 2013.     | doi:10.1016/j.scico.2013.02.001. |       |                 |     |        |       |       |
| ------ | --------------------- | --- | ----------- | ---------- | ----- | --------- | -------------------------------- | ----- | --------------- | --- | ------ | ----- | ----- |
| • Node | rewriting specializes |     | the AST for | the actual | types |           |                                  |       |                 |     |        |       |       |
|        |                       |     |             |            |       | [8] C. F. | Bolz, A.                         | Cuni, | M. Fijałkowski, |     | and A. | Rigo. | Trac- |
used,andcanresultintheelisionofunnecessarygener-
|     |     |     |     |     |     | ing the | meta-level: | PyPy’s | tracing | JIT | compiler. | In  | Pro- |
| --- | --- | --- | --- | --- | --- | ------- | ----------- | ------ | ------- | --- | --------- | --- | ---- |
ality,e.g.,boxing,complexdispatch.
|     |     |     |     |     |     | ceedings | of the | Workshop | on  | the Implementation, |     | Compila- |     |
| --- | --- | --- | --- | --- | --- | -------- | ------ | -------- | --- | ------------------- | --- | -------- | --- |
• Compilation by automatic partial evaluation leads to tion, Optimization of Object-Oriented Languages and Pro-
|     |     |     |     |     |     | gramming | Systems, | pages | 18–25. | ACM | Press, | 2009. | doi: |
| --- | --- | --- | --- | --- | --- | -------- | -------- | ----- | ------ | --- | ------ | ----- | ---- |
highlyoptimizedmachinecodewithouttheneedofwrit-
ingalanguage-specificdynamiccompiler. 10.1145/1565824.1565827.
[9] C.F.Bolz,A.Cuni,M.Fijałkowski,M.Leuschel,S.Pedroni,
• DeoptimizationfrommachinecodebacktotheASTin-
|     |     |     |     |     |     | and A. | Rigo. | Runtime | feedback | in a | meta-tracing |     | JIT for |
| --- | --- | --- | --- | --- | --- | ------ | ----- | ------- | -------- | ---- | ------------ | --- | ------- |
terpreterhandlesspeculationfailures.
|     |     |     |     |     |     | efficientdynamiclanguages. |     |     | InProceedingsoftheWorkshop |     |     |     |     |
| --- | --- | --- | --- | --- | --- | -------------------------- | --- | --- | -------------------------- | --- | --- | --- | --- |
SourcecodefortheGraalcompiler,theTruffleinterpre- ontheImplementation,Compilation,OptimizationofObject-
|                   |            |     |                          |     |     | Oriented           | Languages | and | Programming                  |     | Systems, | pages | 9:1– |
| ----------------- | ---------- | --- | ------------------------ | --- | --- | ------------------ | --------- | --- | ---------------------------- | --- | -------- | ----- | ---- |
| tation framework, | and sample |     | language implementations |     | is  |                    |           |     |                              |     |          |       |      |
|                   |            |     |                          |     |     | 9:8.ACMPress,2011. |           |     | doi:10.1145/2069172.2069181. |     |          |       |      |
availableattheOpenJDKProjectGraalsite[45].
|     |     |     |     |     |     | [10] C.F.Bolz,A.Cuni,M.Fijałkowski,M.Leuschel,S.Pedroni, |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | -------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
Acknowledgments and A. Rigo. Allocation removal by partial evaluation in a
|          |             |        |                 |          |     | tracingJIT. | InProceedingsoftheACMSIGPLANWorkshop |     |     |     |     |     |     |
| -------- | ----------- | ------ | --------------- | -------- | --- | ----------- | ------------------------------------ | --- | --- | --- | --- | --- | --- |
| We thank | all members | of the | Virtual Machine | Research |     |             |                                      |     |     |     |     |     |     |
onPartialEvaluationandProgramManipulation,pages43–
| Group at        | Oracle Labs, | the Institute | for System | Software       |     |                     |     |           |                              |     |                   |     |     |
| --------------- | ------------ | ------------- | ---------- | -------------- | --- | ------------------- | --- | --------- | ---------------------------- | --- | ----------------- | --- | --- |
|                 |              |               |            |                |     | 52.ACMPress,2011.   |     |           | doi:10.1145/1929501.1929508. |     |                   |     |     |
| at the Johannes | Kepler       | University    | Linz, and  | our collabora- |     |                     |     |           |                              |     |                   |     |     |
|                 |              |               |            |                |     | [11] S. Brunthaler. |     | Efficient | interpretation               |     | using quickening. |     | In  |
torsatPurdueUniversity,TUDortmund,andUCIrvinefor
ProceedingsoftheDynamicLanguagesSymposium,pages1–
| their support | and contributions. |     | We especially | thank | Peter |                   |     |     |                              |     |     |     |     |
| ------------- | ------------------ | --- | ------------- | ----- | ----- | ----------------- | --- | --- | ---------------------------- | --- | --- | --- | --- |
|               |                    |     |               |       |       | 14.ACMPress,2010. |     |     | doi:10.1145/1869631.1869633. |     |     |     |     |
Kessler,MichaelVanDeVanter,ChrisSeaton,StephenKell,
|     |     |     |     |     |     | [12] S. Brunthaler. |     | Inline | caching | meets | quickening. | In  | Pro- |
| --- | --- | --- | --- | --- | --- | ------------------- | --- | ------ | ------- | ----- | ----------- | --- | ---- |
andMichaelHauptforfeedbackonthispaper.
|     |     |     |     |     |     | ceedings | of the | European | Conference |     | on Object-Oriented |     |     |
| --- | --- | --- | --- | --- | --- | -------- | ------ | -------- | ---------- | --- | ------------------ | --- | --- |
TheauthorsfromJohannesKeplerUniversityarefunded
|     |     |     |     |     |     | Programming, |     | pages 429–451. |     | Springer-Verlag, |     | 2010. | doi: |
| --- | --- | --- | --- | --- | --- | ------------ | --- | -------------- | --- | ---------------- | --- | ----- | ---- |
inpartbyaresearchgrantfromOracle. 10.1007/978-3-642-14107-2 21.
| Oracle | and Java are | registered | trademarks | of  | Oracle |                |       |       |        |        |            |     |          |
| ------ | ------------ | ---------- | ---------- | --- | ------ | -------------- | ----- | ----- | ------ | ------ | ---------- | --- | -------- |
|        |              |            |            |     |        | [13] K. Casey, | M. A. | Ertl, | and D. | Gregg. | Optimizing |     | indirect |
and/oritsaffiliates.Othernamesmaybetrademarksoftheir branch prediction accuracy in virtual machine interpreters.
respectiveowners.
ACMTransactionsonProgrammingLanguagesandSystems,
|     |     |     |     |     |     | 29(6),2007. | doi:10.1145/1286821.1286828. |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | ----------- | ---------------------------- | --- | --- | --- | --- | --- | --- |
References
|     |     |     |     |     |     | [14] J. Castanos, | D.  | Edelsohn, |     | K. Ishizaki, |     | P. Nagpurkar, |     |
| --- | --- | --- | --- | --- | --- | ----------------- | --- | --------- | --- | ------------ | --- | ------------- | --- |
[1] R.Affeldt,H.Masuhara,E.Sumii,andA.Yonezawa. Sup- T.Nakatani,T.Ogasawara,andP.Wu.Onthebenefitsandpit-
porting objects in run-time bytecode specialization. In Pro- fallsofextendingastaticallytypedlanguageJITcompilerfor
ceedingsoftheASIANSymposiumonPartialEvaluationand dynamicscriptinglanguages.InProceedingsoftheACMSIG-
Semantics-BasedProgramManipulation,pages50–60.ACM PLANConferenceonObject-OrientedProgrammingSystems,
| Press,2002. | doi:10.1145/568173.568179. |     |     |     |     |            |     |               |     |       |          |     |        |
| ----------- | -------------------------- | --- | --- | --- | --- | ---------- | --- | ------------- | --- | ----- | -------- | --- | ------ |
|             |                            |     |     |     |     | Languages, | and | Applications, |     | pages | 195–212. | ACM | Press, |
[2] B.Alpern,S.Augart,S.M.Blackburn,M.Butrico,A.Cocchi, 2012. doi:10.1145/2384616.2384631.
P.Cheng,J.Dolby,S.Fink,D.Grove,M.Hind,K.S.McKin-
|         |               |          |             |            |     | [15] Y.Chiba. | Redundantboxingeliminationbyadynamiccom- |     |     |     |     |     |     |
| ------- | ------------- | -------- | ----------- | ---------- | --- | ------------- | ---------------------------------------- | --- | --- | --- | --- | --- | --- |
| ley, M. | Mergen, J. E. | B. Moss, | T. Ngo, and | V. Sarkar. | The |               |                                          |     |     |     |     |     |     |
pilerforJava.InProceedingsoftheInternationalConference
Jikes Research Virtual Machine project: Building an open- onthePrinciplesandPracticeofProgramminginJava,pages
sourceresearchcommunity.IBMSystemsJournal,44(2):399– 215–220.ACMPress,2007. doi:10.1145/1294325.1294355.
| 417,2005. | doi:10.1147/sj.442.0399. |     |     |     |     |     |     |     |     |     |     |     |     |
| --------- | ------------------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

[16] R.Cytron,J.Ferrante,B.K.Rosen,M.N.Wegman,andF.K. DesignandImplementation,pages32–43.ACMPress,1992.
Zadeck. Efficientlycomputingstaticsingleassignmentform doi:10.1145/143095.143114.
| and the | control | dependence |     | graph. | ACM | Transactions | on  |                  |     |                                   |     |     |     |     |
| ------- | ------- | ---------- | --- | ------ | --- | ------------ | --- | ---------------- | --- | --------------------------------- | --- | --- | --- | --- |
|         |         |            |     |        |     |              |     | [28] HotSpotJVM. |     | Javaversionhistory(J2SE1.3),2013. |     |     |     | URL |
ProgrammingLanguagesandSystems,13(4):451–490,1991.
|     |     |     |     |     |     |     |     | http://en.wikipedia.org/wiki/Java |     |     |     | version | history. |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------------------------------- | --- | --- | --- | ------- | -------- | --- |
doi:10.1145/115372.115320.
|     |     |     |     |     |     |     |     | [29] R. K. | W. Hui, | K. E. Iverson, | E.  | E. McDonnell, |     | and A. T. |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------- | ------- | -------------- | --- | ------------- | --- | --------- |
[17] M. A. Ertl and D. Gregg. Combining stack caching with APL\?
|                                                       |                    |                              |     |             |     |                 |     | Whitney.                              |                 | InConferenceProceedingsonAPL90:for |     |     |       |              |
| ----------------------------------------------------- | ------------------ | ---------------------------- | --- | ----------- | --- | --------------- | --- | ------------------------------------- | --------------- | ---------------------------------- | --- | --- | ----- | ------------ |
| dynamic                                               | superinstructions. |                              | In  | Proceedings |     | of the Workshop |     |                                       |                 |                                    |     |     |       |              |
|                                                       |                    |                              |     |             |     |                 |     | thefuture,pages192–200.ACMPress,1990. |                 |                                    |     |     |       | doi:10.1145/ |
| onInterpreters,VirtualMachinesandEmulators,pages7–14. |                    |                              |     |             |     |                 |     | 97811.97845.                          |                 |                                    |     |     |       |              |
| ACMPress,2004.                                        |                    | doi:10.1145/1059579.1059583. |     |             |     |                 |     |                                       |                 |                                    |     |     |       |              |
|                                                       |                    |                              |     |             |     |                 |     | [30] IBM.                             | Java 2Platform, | StandardEdition,                   |     |     | 2013. | URL https:   |
[18] B.Folliot,I.Piumarta,andF.Riccardi. Adynamicallycon- //www.ibm.com/developerworks/java/jdk/.
| figurable,multi-languageexecutionplatform. |          |          |     |         |     | InProceedings |      |                                                            |     |                                 |     |     |     |     |
| ------------------------------------------ | -------- | -------- | --- | ------- | --- | ------------- | ---- | ---------------------------------------------------------- | --- | ------------------------------- | --- | --- | --- | --- |
|                                            |          |          |     |         |     |               |      | [31] K.Ishizaki,T.Ogasawara,J.Castanos,P.Nagpurkar,D.Edel- |     |                                 |     |     |     |     |
| of the                                     | European | Workshop | on  | Support | for | Composing     | Dis- |                                                            |     |                                 |     |     |     |     |
|                                            |          |          |     |         |     |               |      | sohn,andT.Nakatani.                                        |     | Addingdynamically-typedlanguage |     |     |     |     |
tributedApplications,pages175–181.ACMPress,1998.doi:
supporttoastatically-typedlanguagecompiler:performance
10.1145/319195.319222.
|                   |            |             |            |              |           |            |          | evaluation,analysis,andtradeoffs. |            |            |         | InProceedingsoftheIn- |                  |     |
| ----------------- | ---------- | ----------- | ---------- | ------------ | --------- | ---------- | -------- | --------------------------------- | ---------- | ---------- | ------- | --------------------- | ---------------- | --- |
| [19] D. Frampton, |            | S. M.       | Blackburn, | P.           | Cheng,    | R. J.      | Garner,  |                                   |            |            |         |                       |                  |     |
|                   |            |             |            |              |           |            |          | ternational                       | Conference | on         | Virtual | Execution             | Environments,    |     |
| D. Grove,         | J.         | E. B. Moss, | and        | S. I.        | Salishev. | Demystify- |          |                                   |            |            |         |                       |                  |     |
|                   |            |             |            |              |           |            |          | pages                             | 169–180.   | ACM Press, | 2012.   | doi:                  | 10.1145/2151024. |     |
| ing magic:        | high-level |             | low-level  | programming. |           | In         | Proceed- |                                   |            |            |         |                       |                  |     |
2151047.
ingsoftheInternationalConferenceonVirtualExecutionEn-
|             |       |        |     |        |       |      |          | [32] K.E.Iverson.AProgrammingLanguage.JohnWiley&Sons, |                    |     |     |     |     |     |
| ----------- | ----- | ------ | --- | ------ | ----- | ---- | -------- | ----------------------------------------------------- | ------------------ | --- | --- | --- | --- | --- |
| vironments, | pages | 81–90. | ACM | Press, | 2009. | doi: | 10.1145/ |                                                       |                    |     |     |     |     |     |
|             |       |        |     |        |       |      |          | Inc.,1962.                                            | ISBN0-471430-14-5. |     |     |     |     |     |
1508293.1508305.
|     |     |     |     |     |     |     |     | [33] N.D.JonesandA.J.Glenstrup.Programgeneration,termina- |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------------------------------------------------------- | --- | --- | --- | --- | --- | --- |
[20] B.Fulgham.Thecomputerlanguagebenchmarksgame,2013.
|     |     |     |     |     |     |     |     | tion,andbinding-timeanalysis. |     |     |     | InGenerativeProgramming |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------------------------- | --- | --- | --- | ----------------------- | --- | --- |
URLhttp://benchmarksgame.alioth.debian.org/.
andComponentEngineering,volume2487ofLectureNotes
[21] Y.Futamura.Partialevaluationofcomputationprocess–anap- inComputerScience,pages1–31.Springer-Verlag,2002.doi:
proachtoacompiler-compiler.Systems,Computers,Controls,
|     |     |     |     |     |     |     |     | 10.1007/3-540-45821-2 |     |     | 1.  |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------------------- | --- | --- | --- | --- | --- | --- |
2(5):721–728,1971.
|                |     |               |     |           |                 |            |     | [34] JRuby.JRuby:TheRubyprogramminglanguageontheJVM, |                              |     |     |     |                |     |
| -------------- | --- | ------------- | --- | --------- | --------------- | ---------- | --- | ---------------------------------------------------- | ---------------------------- | --- | --- | --- | -------------- | --- |
| [22] E. Gagnon | and | L. Hendren.   |     | Effective | inline-threaded |            | in- |                                                      |                              |     |     |     |                |     |
|                |     |               |     |           |                 |            |     | 2013.                                                | URLhttp://jruby.org/.        |     |     |     |                |     |
| terpretation   | of  | Java bytecode |     | using     | preparation     | sequences. |     |                                                      |                              |     |     |     |                |     |
|                |     |               |     |           |                 |            |     | [35] JSR223.                                         | JSR223:ScriptingfortheJavaTM |     |     |     | platform,2006. |     |
InProceedingsoftheInternationalConferenceonCompiler
URLhttp://www.jcp.org/en/jsr/detail?id=223.
| Construction,         |                | pages 170–184. |        | Springer-Verlag, |            | 2003.      | doi:     |                                               |        |                      |            |        |            |            |
| --------------------- | -------------- | -------------- | ------ | ---------------- | ---------- | ---------- | -------- | --------------------------------------------- | ------ | -------------------- | ---------- | ------ | ---------- | ---------- |
|                       |                |                |        |                  |            |            |          | [36] T. Kotzmann                              |        | and H. Mo¨ssenbo¨ck. |            | Escape | analysis   | in the     |
| 10.1007/3-540-36579-6 |                |                | 13.    |                  |            |            |          |                                               |        |                      |            |        |            |            |
|                       |                |                |        |                  |            |            |          | contextofdynamiccompilationanddeoptimization. |        |                      |            |        |            | InPro-     |
| [23] N. Geoffray,     |                | G. Thomas,     | J.     | Lawall,          | G. Muller, | and        | B. Fol-  |                                               |        |                      |            |        |            |            |
|                       |                |                |        |                  |            |            |          | ceedings                                      | of the | International        | Conference |        | on Virtual | Execu-     |
| liot.                 | VMKit:         | A substrate    |        | for managed      | runtime    |            | environ- |                                               |        |                      |            |        |            |            |
|                       |                |                |        |                  |            |            |          | tion Environments,                            |        | pages                | 111–120.   | ACM    | Press,     | 2005. doi: |
| ments.                | In Proceedings |                | of the | International    |            | Conference | on       |                                               |        |                      |            |        |            |            |
10.1145/1064979.1064996.
| Virtual | Execution | Environments, |     | pages | 51–62, | 2010. | doi: |     |     |     |     |     |     |     |
| ------- | --------- | ------------- | --- | ----- | ------ | ----- | ---- | --- | --- | --- | --- | --- | --- | --- |
10.1145/1735997.1736006. [37] T. Kotzmann and H. Mo¨ssenbo¨ck. Run-time support for
|                  |     |             |     |         |          |     |         | optimizations |     | based on escape | analysis. |     | In Proceedings | of  |
| ---------------- | --- | ----------- | --- | ------- | -------- | --- | ------- | ------------- | --- | --------------- | --------- | --- | -------------- | --- |
| [24] L. Hendren, |     | J. Doherty, | A.  | Dubrau, | R. Garg, | N.  | Lameed, |               |     |                 |           |     |                |     |
theInternationalSymposiumonCodeGenerationandOpti-
| S. Radpour, | A.         | Aslam, | T. Aslam, | A.        | Casey, | M. C. | Boisvert, |           |       |             |          |     |          |            |
| ----------- | ---------- | ------ | --------- | --------- | ------ | ----- | --------- | --------- | ----- | ----------- | -------- | --- | -------- | ---------- |
|             |            |        |           |           |        |       |           | mization, | pages | 49–60. IEEE | Computer |     | Society, | 2007. doi: |
| J. Li, C.   | Verbrugge, | and    | O. S.     | Belanger. | McLAB: |       | enabling  |           |       |             |          |     |          |            |
10.1109/CGO.2007.34.
| programming        |     | language, | compiler                   | and | software | engineering |     |                                                       |     |     |     |     |     |     |
| ------------------ | --- | --------- | -------------------------- | --- | -------- | ----------- | --- | ----------------------------------------------------- | --- | --- | --- | --- | --- | --- |
|                    |     |           |                            |     |          |             |     | [38] C.LattnerandV.Adve.LLVM:Acompilationframeworkfor |     |     |     |     |     |     |
| researchforMATLAB. |     |           | InCompaniontotheACMSIGPLAN |     |          |             |     |                                                       |     |     |     |     |     |     |
ConferenceonObjectOrientedProgrammingSystems,Lan- lifelongprogramanalysis&transformation. InProceedings
guages,andApplications,pages195–196.ACMPress,2011. oftheInternationalSymposiumonCodeGenerationandOp-
doi:10.1145/2048147.2048203. timization,pages75–86.IEEEComputerSociety,2004. doi:
10.1109/CGO.2004.1281665.
| [25] U.Ho¨lzleandD.Ungar. |     |     | Optimizingdynamically-dispatched |     |     |     |     |     |     |     |     |     |     |     |
| ------------------------- | --- | --- | -------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
callswithrun-timetypefeedback.InProceedingsoftheACM [39] MacRuby. MacRuby,2013. URLhttp://macruby.org/.
SIGPLANConferenceonProgrammingLanguageDesignand [40] M. Marquard and B. Steensgaard. Partial evaluation of an
Implementation,pages326–336.ACMPress,1994. doi:10. object-orientedimperativelanguage. Master’sthesis,DIKU,
| 1145/178243.178478. |     |     |     |     |     |     |     | UniversityofCopenhagen,1992. |     |     |     |     |     |     |
| ------------------- | --- | --- | --- | --- | --- | --- | --- | ---------------------------- | --- | --- | --- | --- | --- | --- |
[26] U. Ho¨lzle, C. Chambers, and D. Ungar. Optimizing [41] H.MasuharaandA.Yonezawa. Aportable-approachtody-
dynamically-typed object-oriented languages with polymor- namic optimization in run-time specialization. New Gen-
phic inline caches. In Proceedings of the European Con- eration Computing, 20(1):101–124, 2002. doi: 10.1007/
| ference | on Object-Oriented |     |     | Programming, |     | pages | 21–38. |     |     |     |     |     |     |     |
| ------- | ------------------ | --- | --- | ------------ | --- | ----- | ------ | --- | --- | --- | --- | --- | --- | --- |
BF03037261.
Springer-Verlag,1991. [42] E.MeijerandJ.Gough. TechnicaloverviewoftheCommon
| [27] U. Ho¨lzle, | C.  | Chambers, | and | D. Ungar. |     | Debugging | opti- |          |          |       |     |                             |     |     |
| ---------------- | --- | --------- | --- | --------- | --- | --------- | ----- | -------- | -------- | ----- | --- | --------------------------- | --- | --- |
|                  |     |           |     |           |     |           |       | Language | Runtime, | 2000. | URL | https://research.microsoft. |     |     |
mizedcodewithdynamicdeoptimization. InProceedingsof com/en-us/um/people/emeijer/papers/clr.pdf.
| the ACM | SIGPLAN | Conference |     | on Programming |     | Language |     |                   |     |             |         |     |           |          |
| ------- | ------- | ---------- | --- | -------------- | --- | -------- | --- | ----------------- | --- | ----------- | ------- | --- | --------- | -------- |
|         |         |            |     |                |     |          |     | [43] F. Morandat, |     | B. Hill, L. | Osvald, | and | J. Vitek. | Evaluat- |
ingthedesignoftheRlanguage—Objectsandfunctionsfor

data analysis. In Proceedings of the European Conference tions, pages 375–390. ACM Press, 2011. doi: 10.1145/
| onObject-OrientedProgramming,pages104–131.Springer- |     |                               |     |     |     |     |     | 2048066.2048098.                        |     |     |     |     |                |     |
| --------------------------------------------------- | --- | ----------------------------- | --- | --- | --- | --- | --- | --------------------------------------- | --- | --- | --- | --- | -------------- | --- |
| Verlag,2012.                                        |     | doi:10.1007/978-3-642-31057-7 |     |     |     | 6.  |     |                                         |     |     |     |     |                |     |
|                                                     |     |                               |     |     |     |     |     | [59] Y.Shi,K.Casey,M.A.Ertl,andD.Gregg. |     |     |     |     | Virtualmachine |     |
[44] R.OdairaandK.Hiraki. SentinelPRE:Hoistingbeyondex- showdown: Stack versus registers. ACM Transactions on
ception dependency with dynamic deoptimization. In Pro- Architecture and Code Optimization, 4(4), 2008. doi: 10.
ceedingsoftheInternationalSymposiumonCodeGeneration 1145/1328195.1328197.
| and | Optimization, | pages | 328–338. |     | IEEE Computer |     | Society, |                                     |     |     |     |                         |     |     |
| --- | ------------- | ----- | -------- | --- | ------------- | --- | -------- | ----------------------------------- | --- | --- | --- | ----------------------- | --- | --- |
|     |               |       |          |     |               |     |          | [60] J.M.SipelsteinandG.E.Blelloch. |     |     |     | Collection-orientedlan- |     |     |
2005. doi:10.1109/CGO.2005.32. guages. ProceedingsoftheIEEE,79(4):504–523,1991.
| [45] OpenJDK.   | Graalproject,2013.   |     |     | URLhttp://openjdk.java.net/ |                         |     |     |                                |     |         |                 |                                  |                  |     |
| --------------- | -------------------- | --- | --- | --------------------------- | ----------------------- | --- | --- | ------------------------------ | --- | ------- | --------------- | -------------------------------- | ---------------- | --- |
|                 |                      |     |     |                             |                         |     |     | [61] Smalltalk/X,              |     | 2013.   | URL             | http://live.exept.de/doc/online/ |                  |     |
| projects/graal. |                      |     |     |                             |                         |     |     | english/programming/java.html. |     |         |                 |                                  |                  |     |
| [46] OpenJDK.   | ProjectSumatra,2013. |     |     |                             | URLhttp://openjdk.java. |     |     |                                |     |         |                 |                                  |                  |     |
|                 |                      |     |     |                             |                         |     |     | [62] L. Stadler,               | C.  | Wimmer, | T. Wu¨rthinger, |                                  | H. Mo¨ssenbo¨ck, | and |
net/projects/sumatra/. J. Rose. Lazy continuations for Java virtual machines. In
[47] I.PiumartaandF.Riccardi. Optimizingdirectthreadedcode ProceedingsoftheInternationalConferenceonthePrinciples
byselectiveinlining. InProceedingsoftheACMSIGPLAN andPracticeofProgramminginJava,pages143–152.ACM
Conference on Programming Language Design and Imple- Press,2009. doi:10.1145/1596655.1596679.
mentation,pages291–300.ACMPress,1998. doi:10.1145/ [63] L. Stadler, T. Wu¨rthinger, and C. Wimmer. Efficient corou-
277650.277743.
|     |     |     |     |     |     |     |     | tines | for the | Java platform. |     | In Proceedings | of  | the Inter- |
| --- | --- | --- | --- | --- | --- | --- | --- | ----- | ------- | -------------- | --- | -------------- | --- | ---------- |
[48] G.Richards,C.Hammer,B.Burg,andJ.Vitek. Theevalthat national Conference on the Principles and Practice of Pro-
mendo:Alarge-scalestudyoftheuseofevalinJavaScript gramming in Java, pages 20–28. ACM Press, 2010. doi:
applications. In Proceedings of the European Conference 10.1145/1852761.1852765.
| on Object-Oriented |     |     | Programming, |     | pages | 52–78. | Springer- |                                                           |     |     |     |     |     |     |
| ------------------ | --- | --- | ------------ | --- | ----- | ------ | --------- | --------------------------------------------------------- | --- | --- | --- | --- | --- | --- |
|                    |     |     |              |     |       |        |           | [64] S.Thibault,C.Consel,J.L.Lawall,R.Marlet,andG.Muller. |     |     |     |     |     |     |
Verlag,2011. doi:10.1007/978-3-642-22655-7 4. Static and dynamic program compilation by interpreter spe-
[49] A.RigoandS.Pedroni. PyPy’sapproachtovirtualmachine cialization.JournalonHigher-OrderandSymbolicComputa-
construction. In Companion to the ACM SIGPLAN Confer- tion,13(3):161–178,2000. doi:10.1023/A:1010078412711.
enceonObjectOrientedProgrammingSystems,Languages, [65] D. Ungar and R. B. Smith. Self: The power of simplic-
| and | Applications, | pages | 944–953. |     | ACM Press, | 2006. | doi: |         |             |     |            |         |            |     |
| --- | ------------- | ----- | -------- | --- | ---------- | ----- | ---- | ------- | ----------- | --- | ---------- | ------- | ---------- | --- |
|     |               |       |          |     |            |       |      | ity. In | Proceedings |     | of the ACM | SIGPLAN | Conference | on  |
10.1145/1176617.1176753.
Object-OrientedProgrammingSystems,Languages,andAp-
[50] River Trail. Parallel EcmaScript (River Trail) API, 2013. plications,pages227–242.ACMPress,1987. doi:10.1145/
| URLhttp://wiki.ecmascript.org/doku.php?id=strawman:data |     |     |     |     |     |     |     | 38765.38828. |     |     |     |     |     |     |
| ------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | ------------ | --- | --- | --- | --- | --- | --- |
parallelism.
|     |     |     |     |     |     |     |     | [66] UnladenSwallow. |     | unladen-swallow,2009. |     |     | URLhttp://code. |     |
| --- | --- | --- | --- | --- | --- | --- | --- | -------------------- | --- | --------------------- | --- | --- | --------------- | --- |
[51] J.R.Rose. Bytecodesmeetcombinators:invokedynamicon google.com/p/unladen-swallow/.
| the JVM. | In  | Proceedings |     | of the ACM | Workshop | on  | Virtual |            |         |           |                |     |           |         |
| -------- | --- | ----------- | --- | ---------- | -------- | --- | ------- | ---------- | ------- | --------- | -------------- | --- | --------- | ------- |
|          |     |             |     |            |          |     |         | [67] R. C. | Waters. | Automatic | transformation |     | of series | expres- |
MachinesandIntermediateLanguages,2009. sions into loops. ACM Transactions on Programming Lan-
[52] Rubinius. Rubinius:UseRuby,2013. URLhttp://rubini.us/. guages and Systems, 13(1):52–98, 1992. doi: 10.1145/
[53] E.RufandD.Weise. Opportunitiesforonlinepartialevalua- 114005.102806.
tion. Technicalreport,StanfordUniversity,1992. [68] C.WimmerandHanspeterMo¨ssenbo¨ck.Automaticfeedback-
|                              |     |     |     |                              |     |     |     | directed | object | fusing. | ACM | Transactions | on  | Architecture |
| ---------------------------- | --- | --- | --- | ---------------------------- | --- | --- | --- | -------- | ------ | ------- | --- | ------------ | --- | ------------ |
| [54] D.SchneiderandC.F.Bolz. |     |     |     | Theefficienthandlingofguards |     |     |     |          |        |         |     |              |     |              |
in the design of RPython’s tracing JIT. In Proceedings of and Code Optimization, 7(2), 2010. doi: 10.1145/1839667.
| the ACM | Workshop |     | on Virtual | Machines |     | and Intermediate |     | 1839669. |     |     |     |     |     |     |
| ------- | -------- | --- | ---------- | -------- | --- | ---------------- | --- | -------- | --- | --- | --- | --- | --- | --- |
Languages, pages 3–12. ACM Press, 2012. doi: 10.1145/ [69] C. Wimmer, M. Haupt, M. L. Van De Vanter, M. Jordan,
2414740.2414743. L.Dayne`s,andD.Simon. Maxine:Anapproachablevirtual
|            |          |       |         |            |     |            |     | machinefor,andin,Java. |     |     | ACMTransactionsonArchitecture |     |     |     |
| ---------- | -------- | ----- | ------- | ---------- | --- | ---------- | --- | ---------------------- | --- | --- | ----------------------------- | --- | --- | --- |
| [55] U. P. | Schultz, | J. L. | Lawall, | C. Consel, | and | G. Muller. | To- |                        |     |     |                               |     |     |     |
andCodeOptimization,9(4):30:1–30:24,2013.doi:10.1145/
| wards | automatic | specialization |     | of  | Java programs. |     | In Pro- |     |     |     |     |     |     |     |
| ----- | --------- | -------------- | --- | --- | -------------- | --- | ------- | --- | --- | --- | --- | --- | --- | --- |
2400682.2400689.
| ceedings | of  | the European |     | Conference | on  | Object-Oriented |     |     |     |     |     |     |     |     |
| -------- | --- | ------------ | --- | ---------- | --- | --------------- | --- | --- | --- | --- | --- | --- | --- | --- |
Programming, pages 367–390. Springer-Verlag, 1999. doi: [70] M.Wolczko,O.Agesen,andD.Ungar. Towardsauniversal
10.1007/3-540-48743-3 17. implementation substrate for object-oriented languages. In
[56] U.P.Schultz,J.L.Lawall,andC.Consel.Automaticprogram ProceedingsoftheWorkshoponSimplicity,Performanceand
specialization for Java. In ACM Transactions on Program- PortabilityinVirtualMachineDesign,1999.
ming Languages and Systems, pages 452–499. ACM Press, [71] T. Wu¨rthinger, C. Wimmer, and H. Mo¨ssenbo¨ck. Array
2003. doi:10.1145/778559.778561. bounds check elimination in the context of deoptimization.
|                      |     |                                      |     |     |     |     |     | Science | of Computer |     | Programming, |     | 74(5-6), | 2009. doi: |
| -------------------- | --- | ------------------------------------ | --- | --- | --- | --- | --- | ------- | ----------- | --- | ------------ | --- | -------- | ---------- |
| [57] A.Schwaighofer. |     | TailcalloptimizationintheJavaHotSpot |     |     |     |     |     |         |             |     |              |     |          |            |
10.1016/j.scico.2009.01.002.
VM. Master’sthesis,JohannesKeplerUniversityLinz,2009.
|               |             |        |             |            |            |             |          | [72] T. Wu¨rthinger, |         | A. Wo¨ß,        | L. Stadler,                  | G.  | Duboscq,      | D. Simon, |
| ------------- | ----------- | ------ | ----------- | ---------- | ---------- | ----------- | -------- | -------------------- | ------- | --------------- | ---------------------------- | --- | ------------- | --------- |
| [58] A. Shali | and         | W. R.  | Cook.       | Hybrid     | partial    | evaluation. | In       |                      |         |                 |                              |     |               |           |
|               |             |        |             |            |            |             |          | and C.               | Wimmer. | Self-optimizing |                              | AST | interpreters. | In Pro-   |
| Proceedings   |             | of the | ACM SIGPLAN |            | Conference | on          | Object-  |                      |         |                 |                              |     |               |           |
|               |             |        |             |            |            |             |          | ceedings             | of the  | Dynamic         | Languages                    |     | Symposium,    | pages 73– |
| Oriented      | Programming |        | Systems,    | Languages, |            | and         | Applica- |                      |         |                 |                              |     |               |           |
|               |             |        |             |            |            |             |          | 82.ACMPress,2012.    |         |                 | doi:10.1145/2384577.2384587. |     |               |           |