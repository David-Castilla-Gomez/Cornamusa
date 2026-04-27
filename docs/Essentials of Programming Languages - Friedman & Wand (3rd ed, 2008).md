ESSENTIALS OF
PROGRAMMING
THIRD EDITION
LANGUAGES
Daniel P. Friedman and Mitchell Wand
PROGRAMMING
LANGUAGES
THIRD
EDITION
Friedman
and
Wand
ESSENTIALS
OF
MD
DALIM
955472
3/22/08
CYAN
MAG
YELO
BLACK
computer science/programming languages
Essentials of Programming Languages
third edition
Daniel P. Friedman and Mitchell Wand
This book provides students with a deep, working understanding of the essential concepts of program-
ming languages. Most of these essentials relate to the semantics, or meaning, of program elements,
and the text uses interpreters (short programs that directly analyze an abstract representation of the
program text) to express the semantics of many essential language elements in a way that is both clear
and executable. The approach is both analytical and hands-on. The book provides views of program-
ming languages using widely varying levels of abstraction, maintaining a clear connection between the
high-level and low-level views. Exercises are a vital part of the text and are scattered throughout; the text
explains the key concepts, and the exercises explore alternative designs and other issues. The complete
Scheme code for all the interpreters and analyzers in the book can be found online through The MIT
Press website.
For this new edition, each chapter has been revised and many new exercises have been added.
Significant additions have been made to the text, including completely new chapters on modules and
continuation-passing style. Essentials of Programming Languages can be used for both graduate and un-
dergraduate courses, and for continuing education courses for programmers.
Daniel P. Friedman is Professor of Computer Science at Indiana University and is the author of many
books published by The MIT Press, including The Little Schemer (fourth edition, 1995), The Seasoned
Schemer (1995), A Little Java, A Few Patterns (1997), each of these coauthored with Matthias Felleisen,
and The Reasoned Schemer (2005), coauthored with William E. Byrd and Oleg Kiselyov. Mitchell Wand is
Professor of Computer Science at Northeastern University.
“With lucid prose and elegant code, this book provides the most concrete introduction to the few build-
ing blocks that give rise to a wide variety of programming languages. I recommend it to my students and
look forward to using it in my courses.”
—Chung-chieh Shan, Department of Computer Science, Rutgers University
“Having taught from EOPL for several years, I appreciate the way it produces students who understand
the terminology and concepts of programming languages in a deep way, not just from reading about the
concepts, but from programming them and experimenting with them. This new edition has an increased
emphasis on types as contracts for defining procedure interfaces, which is quite important for many
students.”
—Gary T. Leavens, School of Electrical Engineering and Computer Science, University of Central Florida
“I’ve found the interpreters-based approach for teaching programming languages to be both compelling
and rewarding for my students. Exposing students to the revelation that an interpreter for a program-
ming language is itself just another program opens up a world of possibilities for problem solving. The
third edition of Essentials of Programming Languages makes this approach of writing interpreters more
accessible than ever.”
—Marc L. Smith, Department of Computer Science, Vassar College
978-0-262-06279-4
The MIT Press
Massachusetts Institute of Technology
Cambridge, Massachusetts 02142
http://mitpress.mit.edu

Essentials of
Programming
Languages
third edition

Essentials of
Programming
Languages
third edition
Daniel P. Friedman
Mitchell Wand
TheMITPress
Cambridge,Massachusetts
London,England

©2008DanielP.FriedmanandMitchellWand
All rights reserved. No part of this book may be reproduced in any form by any
electronicormechanicalmeans(includingphotocopying,recording,orinformation
storageandretrieval)withoutpermissioninwritingfromthepublisher.
MITPressbooksmaybepurchasedatspecialquantitydiscountsforbusinessorsales
promotional use. For information, please email special_sales@mitpress.mit.eduor
write to Special Sales Department, The MIT Press, 55 Hayward Street, Cambridge,
MA02142.
ThisbookwassetinLATEX2εbytheauthors,andwasprintedandboundintheUnited
StatesofAmerica.
LibraryofCongressCataloging-in-PublicationData
Friedman,DanielP.
Essentialsofprogramminglanguages/DanielP.Friedman,Mitchell
Wand.
—3rded.
p. cm.
Includesbibliographicalreferencesandindex.
ISBN978-0-262-06279-4 (hbk.:alk.paper)
1.ProgrammingLanguages(Electroniccomputers).I.Wand,
Mitchell.II.Title.
QA76.7.F73 2008
005.1—dc22 2007039723
10987654321

Contents
ForewordbyHalAbelson ix
Preface xv
Acknowledgments xxi
1 InductiveSetsofData 1
1.1RecursivelySpecifiedData 1
1.2DerivingRecursivePrograms 12
1.3AuxiliaryProceduresandContextArguments 22
1.4Exercises 25
2 DataAbstraction 31
2.1SpecifyingDataviaInterfaces 31
2.2RepresentationStrategiesforDataTypes 35
2.3InterfacesforRecursiveDataTypes 42
2.4AToolforDefiningRecursiveDataTypes 45
2.5AbstractSyntaxandItsRepresentation 51
3 Expressions 57
3.1SpecificationandImplementationStrategy 57
3.2LET:ASimpleLanguage 60
3.3PROC:ALanguagewithProcedures 74
3.4LETREC:ALanguagewithRecursiveProcedures 82
3.5ScopingandBindingofVariables 87
3.6EliminatingVariableNames 91
3.7ImplementingLexicalAddressing 93

vi Contents
4 State 103
4.1ComputationalEffects 103
4.2EXPLICIT-REFS:ALanguagewithExplicitReferences 104
4.3IMPLICIT-REFS:ALanguagewithImplicitReferences 113
4.4MUTABLE-PAIRS:ALanguagewithMutablePairs 124
4.5Parameter-PassingVariations 130
5 Continuation-PassingInterpreters 139
5.1AContinuation-PassingInterpreter 141
5.2ATrampolinedInterpreter 155
5.3AnImperativeInterpreter 160
5.4Exceptions 171
5.5Threads 179
6 Continuation-PassingStyle 193
6.1WritingProgramsinContinuation-PassingStyle 193
6.2TailForm 203
6.3ConvertingtoContinuation-PassingStyle 212
6.4ModelingComputationalEffects 226
7 Types 233
7.1ValuesandTheirTypes 235
7.2AssigningaTypetoanExpression 238
7.3CHECKED:AType-CheckedLanguage 240
7.4INFERRED:ALanguagewithTypeInference 248
8 Modules 275
8.1TheSimpleModuleSystem 276
8.2ModulesThatDeclareTypes 292
8.3ModuleProcedures 311
9 ObjectsandClasses 325
9.1Object-OrientedProgramming 326
9.2Inheritance 329
9.3TheLanguage 334
9.4TheInterpreter 336
9.5ATypedLanguage 352
9.6TheTypeChecker 358

Contents vii
A ForFurtherReading 373
B TheSLLGENParsingSystem 379
B.1Scanning 379
B.2Parsing 382
B.3ScannersandParsersinSLLGEN 383
Bibliography 393
Index 401

Foreword
This book brings you face-to-face with the most fundamental idea in com-
puterprogramming:
Theinterpreter foracomputerlanguageisjustanotherprogram.
It sounds obvious, doesn’t it? But the implications are profound. If you
are a computational theorist, the interpreter idea recalls Gödel’s discovery
of the limitations of formal logical systems, Turing’s concept of a universal
computer,andvonNeumann’sbasicnotionofthestored-programmachine.
Ifyou area programmer,mastering the ideaof aninterpreterisa sourceof
greatpower. It provokes a realshift in mindset, a basic change in the way
youthinkaboutprogramming.
IdidalotofprogrammingbeforeIlearnedaboutinterpreters,andIpro-
duced some substantial programs. One of them, for example, was a large
data-entryandinformation-retrievalsystemwritteninPL/I.WhenIimple-
mented my system, I viewed PL/I as a fixed collection of rules established
bysome unapproachablegroupoflanguagedesigners. Isawmyjob asnot
tomodifytheserules,oreventounderstandthemdeeply,butrathertopick
through the (very) large manual, selecting this or that feature to use. The
notionthattherewassomeunderlyingstructuretothewaythelanguagewas
organized, and thatI might want tooverride some of the language design-
ers’decisions,neveroccurredtome. Ididn’tknowhowtocreateembedded
sublanguages to help organize my implementation, so the entire program
seemed like a large, complex mosaic, where each piece had to be carefully
shaped and fitted into place, rather than a cluster of languages, where the
piecescouldbeflexiblycombined. Ifyoudon’tunderstandinterpreters,you
canstillwriteprograms;youcanevenbeacompetentprogrammer.Butyou
can’tbeamaster.

x Foreword
There are three reasons why as a programmer you should learn about
interpreters.
First,youwillneedatsome pointtoimplementinterpreters,perhapsnot
interpretersfor full-blown general-purpose languages, but interpreters just
thesame. Almosteverycomplexcomputersystemwithwhichpeopleinter-
act in flexible ways—a computer drawing tool or an information-retrieval
system, for example—includes some sort of interpreter that structures the
interaction. These programs may include complex individual operations—
shading a region on the display screen, or performing a database search—
but the interpreter is the glue that lets you combine individual operations
intousefulpatterns. Canyouusetheresultofoneoperationastheinputto
another operation? Can you name a sequence of operations? Is the name
local or global? Can you parameterize a sequence of operations, and give
namesto its inputs? And so on. Nomatter how complex and polished the
individualoperationsare,itisoftenthequalityofthegluethatmostdirectly
determinesthepowerofthesystem. It’seasytofindexamplesofprograms
withgoodindividualoperations,butlousyglue;lookingbackonit,Icansee
thatmyPL/Idatabaseprogramcertainlyhadlousyglue.
Second, even programs that are not themselves interpreters have impor-
tant interpreter-like pieces. Look inside a sophisticated computer-aided
designsystemandyou’relikelytofindageometricrecognitionlanguage,a
graphicsinterpreter,arule-basedcontrolinterpreter,andanobject-oriented
language interpreter all working together. One of the most powerful ways
tostructureacomplexprogramisasacollectionoflanguages,eachofwhich
provides a different perspective, a different way of working with the pro-
gramelements. Choosing the right kind of language for the right purpose,
and understanding the implementation tradeoffsinvolved: that’s what the
studyofinterpretersisabout.
Thethirdreasonforlearningaboutinterpretersisthatprogrammingtech-
niquesthatexplicitlyinvolvethestructureoflanguagearebecomingincreas-
ingly important. Today’s concern with designing and manipulating class
hierarchiesinobject-orientedsystemsisonlyoneexampleofthistrend.Per-
haps this is an inevitable consequence of the fact that our programs are
becoming increasingly complex—thinking more explicitly about languages
may be our best tool for dealing with this complexity. Consider again the
basicidea: the interpreteritselfisjustaprogram. Butthatprogramiswrit-
ten in some language, whose interpreter is itself just a program written
...
in some language whose interpreter is itself Perhaps the whole distinc-
tionbetweenprogramandprogramminglanguageisamisleadingidea,and

Foreword xi
futureprogrammerswillseethemselvesnotaswritingprogramsinparticu-
lar,butascreatingnewlanguagesforeachnewapplication.
FriedmanandWandhavedonealandmarkjob,andtheirbookwillchange
the landscape of programming-language courses. They don’t just tell you
aboutinterpreters;theyshowthemtoyou. Thecoreofthebookisatourde
force sequence of interpretersstarting with an abstracthigh-level language
and progressively making linguistic features explicit until we reach a state
machine.Youcanactuallyrunthiscode,studyandmodifyit,andchangethe
waytheseinterpretershandlescoping,parameter-passing,controlstructure,
etc.
Havingusedinterpreterstostudytheexecutionoflanguages,theauthors
show how the same ideas can be used to analyze programs without run-
ningthem. Intwonewchapters,theyshowhowtoimplementtypecheckers
and inferencers, and how these features interact in modern object-oriented
languages.
Partofthereasonfortheappealofthisapproachisthattheauthorshave
chosenagoodtool—theSchemelanguage,whichcombinestheuniformsyn-
tax and data-abstraction capabilities of Lisp with the lexical scoping and
blockstructureofAlgol. Butapowerfultoolbecomesmostpowerfulinthe
handsofmasters. Thesampleinterpretersinthisbookareoutstandingmod-
els. Indeed, since they arerunnable models, I’m sure that these interpreters
and analyzerswill find themselves at the coresof many programming sys-
temsoverthecomingyears.
This is not an easy book. Mastery of interpreters does not come easily,
andforgoodreason. Thelanguagedesignerisafurtherlevelremovedfrom
the end user than is the ordinaryapplicationprogrammer. In designing an
applicationprogram,youthinkaboutthespecifictaskstobeperformed,and
considerwhatfeaturestoinclude. Butindesigningalanguage,youconsider
the various applications people might want to implement, and the ways in
which they might implement them. Should your language have static or
dynamic scope, or a mixture? Should it have inheritance? Should it pass
parameters by reference or by value? Should continuations be explicit or
implicit? Italldependsonhowyouexpectyourlanguagetobeused,which
kindsofprogramsshouldbeeasytowrite,andwhichyoucanaffordtomake
moredifficult.
Also,interpretersreallyaresubtleprograms. Asimplechangetoalineof
code in an interpreter can make an enormous difference in the behavior of
theresultinglanguage. Don’tthinkthatyoucanjustskimtheseprograms—
very few people in the world can glance at a new interpreter and predict

xii Foreword
fromthathow it will behaveevenon relativelysimple programs. Sostudy
theseprograms. Betteryet,runthem—thisisworkingcode. Tryinterpreting
some simple expressions, then more complex ones. Add error messages.
Modify the interpreters. Design your own variations. Try to really master
theseprograms,notjustgetavaguefeelingforhowtheywork.
Ifyoudothis,youwillchangeyourviewofyourprogramming,andyour
viewofyourselfasaprogrammer. You’llcometoseeyourselfasadesigner
oflanguagesratherthanonlyauseroflanguages,asapersonwhochooses
therulesbywhichlanguagesareputtogether,ratherthanonlyafollowerof
rulesthatotherpeoplehavechosen.
PostscripttotheThirdEdition
Theforewordabovewaswrittenonlysevenyearsago. Sincethen,informa-
tion applications and services have entered the lives of people around the
world in ways that hardly seemed possible in 1990. They are powered by
an ever—growing collection of programming languages and programming
frameworks—allerectedonanever-expandingplatformofinterpreters.
Do you want to create Web pages? In 1990, that meant formatting static
text and graphics, in effect, creating a program to be run by browsers exe-
cuting only a single “print” statement. Today’s dynamic Web pages make
fulluseofscriptinglanguages(anothernameforinterpretedlanguages)like
Javascript. The browser programs can be complex, and including asyn-
chronouscallstoaWebserverthatistypicallyrunningaprograminacom-
pletely differentprogramming framework possibly with a host of services,
eachwithitsownindividuallanguage.
Or you might be creating a bot for enhancing the performance of your
avatarinamassiveonline multiplayergamelikeWorldofWarcraft. Inthat
case, you’re probablyusing a scripting language like Lua, possibly with an
object-orientedextensiontohelpinexpressingclassesofbehaviors.
Ormaybeyou’reprogrammingamassivecomputingclustertodoindex-
ing and searching on a global scale. If so, you might be writing your pro-
gramsusingthemap-reduceparadigmoffunctionalprogrammingtorelieve
youofdealingexplicitlywiththedetailsofhowtheindividualprocessorsare
scheduled.

Foreword xiii
Or perhaps you’re developing new algorithms for sensor networks, and
exploringtheuseoflazyevaluationtobetterdealwithparallelismanddata
aggregation. OrexploringtransformationsystemslikeXSLTforcontrolling
Webpages. Ordesigningframeworksfortransformingandremixingmulti-
mediastreams. Or...
Somanynewapplications! Somanynewlanguages! Somanynewinter-
preters!
As ever, novice programmers, even capable ones, can get along viewing
eachnewframeworkindividually,workingwithinitsfixedsetofrules. But
creating new frameworks requires skills of the master: understanding the
principles that run across languages, appreciatingwhich language features
arebestsuitedforwhichtypeofapplication, andknowing howtocraftthe
interpretersthat bring these languages to life. These are the skills you will
learnfromthisbook.
HalAbelson
Cambridge,Massachusetts
September2007

Preface
Goal
This book is an analytic study of programming languages. Our goal is to
provideadeep,workingunderstandingoftheessentialconceptsofprogram-
minglanguages.Theseessentialshaveprovedtobeofenduringimportance;
they form a basis for understanding future developments in programming
languages.
Most of these essentials relate to the semantics, or meaning, of program
elements. Such meanings reflect how program elements are interpreted as
theprogramexecutes. Programscalledinterpretersprovidethemostdirect,
executable expression of program semantics. They process a program by
directlyanalyzinganabstractrepresentationoftheprogramtext. Wethere-
forechooseinterpretersasourprimaryvehicleforexpressingthesemantics
ofprogramminglanguageelements.
Themostinterestingquestionaboutaprogramasobjectis,“Whatdoesit
do?” Thestudyof interpreterstellsusthis. Interpretersarecriticalbecause
they reveal nuances of meaning, and are the direct path to more efficient
compilationandtootherkindsofprogramanalyses.
Interpretersarealsoillustrativeofabroadclassofsystemsthattransform
information from one form to another based on syntax structure. Compil-
ers, for example, transform programs into forms suitable for interpretation
by hardware or virtual machines. Though general compilation techniques
are beyond the scope of this book, we do develop several elementary pro-
gram translation systems. These reflect forms of program analysis typical
of compilation, suchascontrol transformation, variablebinding resolution,
andtypechecking.

xvi Preface
Thefollowingaresomeofthestrategiesthatdistinguishourapproach.
1. Eachnewconceptisexplainedthroughtheuseofasmalllanguage.These
languagesareoftencumulative: laterlanguagesmayrelyonthefeatures
ofearlierones.
2. Language processors such as interpreters and type checkers are used to
explainthebehaviorofprogramsinagivenlanguage. Theyexpresslan-
guagedesigndecisionsinamannerthatisbothformal(unambiguousand
complete)andexecutable.
3. When appropriate, we use interfaces and specifications to create data
abstractions. In this way, we can change data representation without
changing programs. We use this to investigate alternative implementa-
tionstrategies.
4. Ourlanguageprocessorsarewrittenbothattheveryhighlevelneededto
produceaconciseandcomprehensibleviewofsemanticsandatthemuch
lowerlevelneededtounderstandimplementationstrategies.
5. We show how simple algebraic manipulation can be used to predict the
behaviorofprogramsandtoderivetheirproperties.Ingeneral,however,
we make little use of mathematicalnotation, preferringinstead to study
thebehaviorofprogramsthatconstitutetheimplementationsofourlan-
guages.
6. Thetextexplainsthekeyconcepts,whiletheexercisesexplorealternative
designsandotherissues. Forexample,thetextdealswithstaticbinding,
but dynamic binding is discussed in the exercises. One thread of exer-
cises applies the concept of lexical addressing to the various languages
developedinthebook.
Weprovideseveralviewsof programminglanguagesusingwidelyvary-
ing levels of abstraction. Frequently our interpreters provide a very high-
levelview that expresseslanguage semantics in a very concise fashion, not
far from that of formal mathematical semantics. At the other extreme, we
demonstratehowprogramsmaybetransformedintoaverylow-levelform
characteristic of assembly language. By accomplishing this transformation
in small stages, we maintain a clear connection between the high-level and
low-levelviews.

Preface xvii
We have made some significant changes to this edition. We have includ-
ed informal contracts with all nontrivial definitions. This has the effect of
clarifying the chosen abstractions. In addition, the chapter on modules is
completelynew. Tomakeimplementationssimpler,thesourcelanguagefor
chapters 3, 4, 5, 7, and 8 assumes that exactly one argument can be passed
to a function; we have included exercisesthat support multiargument pro-
cedures. Chapter6iscompletelynew, since wehaveoptedforafirst-order
compositional continuation-passing-style transform rather than a relational
one. Also,becauseofthenatureoftail-formexpressions,weusemultiargu-
mentprocedureshere,andintheobjectsandclasseschapter,wedothesame,
thoughthereitisnotsonecessary.Everychapterhasbeenrevisedandmany
newexerciseshavebeenadded.
Organization
The first two chapters provide the foundations for a careful study of pro-
gramminglanguages. Chapter1emphasizestheconnectionbetweeninduc-
tive data specification and recursive programming and introduces several
notions related to the scope of variables. Chapter 2 introduces a data type
facility. Thisleadstoadiscussionofdataabstractionandexamplesofrepre-
sentationaltransformationsofthesortusedinsubsequentchapters.
Chapter3usesthesefoundationstodescribethebehaviorofprogramming
languages. Itintroducesinterpretersasmechanismsforexplainingtherun-
timebehavioroflanguagesanddevelopsaninterpreterforasimple,lexically
scopedlanguagewithfirst-classproceduresandrecursion.Thisinterpreteris
thebasisformuchofthematerialintheremainderofthebook. Thechapter
endsbygivingathoroughtreatmentofalanguagethatusesindicesinplace
ofvariablesandasaresultvariablelookupcanbeviaalistreference.
Chapter 4 introduces a new component, the state, which maps locations
tovalues. Oncethisisadded,wecanlookatvariousquestionsofrepresen-
tation. In addition, it permits us to explore call-by-reference,call-by-name,
andcall-by-needparameter-passingmechanisms.
Chapter5rewritesourbasicinterpreterincontinuation-passingstyle. The
control structure that is needed to run the interpreter thereby shifts from
recursiontoiteration.Thisexposesthecontrolmechanismsoftheinterpreted
language,andstrengthensone’sintuitionforcontrolissuesingeneral.Italso
allows us to extend the language with trampolining, exception-handling,
andmultithreadingmechanisms.

xviii Preface
Chapter 6 is the companion to the previous chapter. There we show
how to transform our familiar interpreter into continuation-passing style;
here we show how to accomplish this for a much larger class of programs.
Continuation-passingstyleisapowerfulprogrammingtool,foritallowsany
sequential control mechanism to be implemented in almost any language.
Thealgorithmisalsoanexampleofanabstractlyspecifiedsource-to-source
programtransformation.
Chapter7turnsthelanguageofchapter3intoatypedlanguage. Firstwe
implementatypechecker.Thenweshowhowthetypesinaprogramcanbe
deducedbyaunification-basedtypeinferencealgorithm.
Chapter 8 builds typed modules relying heavily on an understanding of
the previous chapter. Modules allow us to build and enforce abstraction
boundaries,andtheyofferanewkindofscoping.
Chapter 9 presents the basic concepts of object-oriented languages, cen-
tered on classes. We first develop an efficient run-time architecture, which
is used as the basis for the material in the second part of the chapter. The
secondpartcombinestheideasofthetypecheckerofchapter7withthoseof
theobject-orientedlanguageofthefirstpart,leadingtoaconventionaltyped
object-orientedlanguage. Thisrequiresintroducingnewconceptsincluding
interfaces,abstractmethods,andcasting.
ForFurtherReadingexplainswhereeachoftheideasinthebookhascome
from.Thisisapersonalwalk-throughallowingthereadertheopportunityto
visiteachtopicfromthe originalpaper,thoughinsome cases,we havejust
chosenanaccessiblesource.
Finally,appendixBdescribesourSLLGENparsingsystem.
Thedependenciesofthevariouschaptersareshowninthefigurebelow.

Preface xix
Usage
This material has been used in both undergraduate and graduate courses.
Also,ithasbeenusedincontinuingeducationcoursesforprofessionalpro-
grammers.Weassumebackgroundindatastructuresandexperiencebothin
aprocedurallanguagesuchasC,C++,orJava,andinScheme,ML,Python,
orHaskell.
Exercises are a vital part of the text and are scattered throughout. They
(cid:3)
rangeindifficultyfrombeingtrivialifrelatedmaterialisunderstood[ ],to
(cid:3)(cid:3)(cid:3)
requiring many hours of thought and programming work [ ]. A great
dealofmaterialofapplied,historical,andtheoreticalinterestresideswithin
them. Werecommendthateachexercisebereadandsomethoughtbegiven
as to how to solve it. Although we write our program interpretation and
transformation systems in Scheme, any language that supports both first-
classproceduresandassignment(ML,CommonLisp,Python,Ruby,etc.) is
adequateforworkingtheexercises.
(cid:3)
Exercise0.1 [ ] Weoftenusephraseslike“somelanguageshaveproperty X.” For
eachsuchphrase,findoneormorelanguagesthathavethepropertyandoneormore
languagesthatdonothavetheproperty.Feelfreetoferretoutthisinformationfrom
anydescriptivebookonprogramminglanguages(sayScott(2005),Sebesta(2007),or
Pratt&Zelkowitz(2001)).
Thisisahands-onbook: everythingdiscussedinthebookmaybeimple-
mentedwithinthelimitsofatypicaluniversitycourse. Becausetheabstrac-
tion facilitiesof functional programming languages are especially suited to
thissortofprogramming,wecanwritesubstantiallanguage-processingsys-
tems that are nevertheless compact enough that one can understand and
manipulatethemwithreasonableeffort.
The web site, available through the publisher, includes complete Scheme
codeforalloftheinterpretersandanalyzersinthisbook. Thecodeiswrit-
teninPLTScheme. WechosethisSchemeimplementationbecauseitsmod-
ulesystemandprogrammingenvironmentprovideasubstantialadvantage
to the student. The code is largely R5RS-compatible, and should be easily
portabletoanyfull-featuredSchemeimplementation.

Acknowledgments
We are indebted to countless colleagues and students who used and cri-
tiquedthefirsttwoeditionsofthisbookandprovidedinvaluableassistance
in the long gestation of this third edition. We are especially grateful for
the contributions of the following individuals, to whom we offer a special
wordofthanks. OlivierDanvyencouragedourconsiderationofafirst-order
compositional continuation-passing algorithm andproposedsome interest-
ing exercises. Matthias Felleisen’s keen analysis has improved the design
of several chapters. Amr Sabry made many useful suggestions and found
at least one extremely subtle bug in a draft of chapter 9. Benjamin Pierce
offeredanumberofinsightfulobservationsafterteachingfromthefirstedi-
tion, almost all of which we have incorporated. Gary Leavens provided
exceptionally thorough and valuable comments on early drafts of the sec-
ond edition, including a large number of detailed suggestions for change.
Stephanie Weirichfound a subtle bugin the type inference code of the sec-
ondeditionofchapter7. RyanNewton,inadditiontoreadingadraftofthe
secondedition,assumedtheoneroustaskofsuggestingadifficultylevelfor
eachexerciseforthatedition. Chung-chiehShantaughtfromanearlydraft
ofthethirdeditionandprovidedcopiousandusefulcomments.
KevinMillikin,ArthurLee,RogerKirchner,MaxHailperin,andErikHils-
dale all used early drafts of the second edition. Will Clinger, Will Byrd,
Joe Near, and Kyle Blocher all used drafts of this edition. Their comments
have been extremely valuable. Ron Garcia, Matthew Flatt, Shriram Krish-
namurthi, Steve Ganz, Gregor Kiczales, Marlene Miller, Galen Williamson,
Dipanwita Sarkar, Steven Bogaerts, Albert Rossi, Craig Citro, Christopher
Dutchyn, Jeremy Siek, and Neil Ching also provided careful reading and
usefulcomments.

xxii Acknowledgments
Severalpeopledeservespecialthanksforassistinguswiththisbook. We
want to thank Neil Ching for developing the index. Jonathan Sobel and
ErikHilsdalebuiltseveralprototypeimplementationsandcontributedmany
ideas as we experimented with the design of the define-datatype and
cases syntactic extensions. The Programming Language Team, and espe-
ciallyMatthiasFelleisen, MatthewFlatt, RobbyFindler, andShriramKrish-
namurthi,wereveryhelpfulinprovidingcompatibilitywiththeirDrScheme
system. KentDybvigdevelopedtheexceptionallyefficientandrobustChez
Scheme implementation, which the authors have used for decades. Will
Byrdhasprovidedinvaluableassistanceduringtheentireprocess. Matthias
Felleisen strongly urged us to adopt compatibility with DrScheme’s mod-
ule system, which is evident in the implementation that can be found at
http://mitpress.mit.edu/eopl3.
Some have earned special mention for their thoughtfulness and concern
for our well-being. George Springer and Larry Finkelstein have each sup-
plied invaluable support. Bob Prior, our wonderful editor at MIT Press,
deserves special thanks for his encouragement in getting us to attack the
writingofthisedition. AdaBrunstein,Bob’ssuccessor,alsodeservesthanks
formakingourtransitiontoaneweditorsosmoothly. IndianaUniversity’s
School of Informatics and Northeastern University’s College of Computer
andInformationSciencehavecreatedanenvironmentthathasallowedusto
undertake this project. Mary Friedman’s gracious hosting of severalweek-
longwritingsessionsdidmuchtoaccelerateourprogress.
WewanttothankChristopherT.Haynesforhiscollaborationonthefirst
twoeditions. Unfortunately,hisinterestshaveshiftedelsewhere,andhehas
notcontinuedwithusonthisedition.
Finally,wearemostgratefultoourfamiliesfortoleratingourpassionfor
workingonthebook. ThankyouRob,Shannon,Rachel,Sara,andMary;and
thank you Rebecca and Joshua, Jennifer and Stephen, Joshua and Georgia,
andBarbara.
This edition has been in the works for a while and we have likely over-
looked someone who has helped along the way. We regret any oversight.
You see this written in books all the time and wonder why anyone would
writeit. Ofcourse,youregretanyoversight. But,whenyouhaveanarmyof
helpers(ittakesavillage),youreallyfeelasenseofobligationnottoforget
anyone. So,ifyouwereoverlooked,wearetrulysorry.
—D.P.F.andM.W.

1
Inductive Sets of Data
This chapter introducesthe basic programming tools we will need to write
interpreters,checkersandsimilarprogramsthatformtheheartofaprogram-
minglanguageprocessor.
Becausethesyntaxofaprograminalanguageisusuallyanestedortree-
like structure, recursion will be at the core of our techniques. Section 1.1
andsection1.2introducemethodsforinductivelyspecifyingdatastructures
and show how such specifications may be used to guide the construction
of recursive programs. Section 1.3 shows how to extend these techniques
tomore complex problems. The chapterconcludeswith anextensive setof
exercises. Theseexercisesaretheheartofthischapter. Theyprovideexperi-
encethatisessentialformasteringthetechniqueofrecursiveprogramming
uponwhichtherestofthisbookisbased.
1.1 RecursivelySpecified Data
Whenwritingcodeforaprocedure,wemustknowpreciselywhatkindsof
valuesmayoccur asargumentstotheprocedure,andwhatkindsof values
arelegalfortheproceduretoreturn. Oftenthesesetsofvaluesarecomplex.
Inthissectionweintroduceformaltechniquesforspecifyingsetsofvalues.
1.1.1 InductiveSpecification
Inductivespecificationisa powerful method of specifying a setof values. To
illustratethismethod,weuseittodescribeacertainsubset Softhenatural
={ , , ,...}
numbersN 0 1 2 .

| 2               |                                 |     |     |     |     | 1 InductiveSetsofData |     |
| --------------- | ------------------------------- | --- | --- | --- | --- | --------------------- | --- |
| Definition1.1.1 | AnaturalnumbernisinSifandonlyif |     |     |     |     |                       |     |
=
1. n 0,or
− ∈
| 2. n 3 S. |     |     |     |     |     |     |     |
| --------- | --- | --- | --- | --- | --- | --- | --- |
Letusseehowwecanusethisdefinitiontodeterminewhatnaturalnum-
bers are in S. We know that 0 ∈ S. Therefore 3 ∈ S, since (3 − 3) = 0 and
| ∈               | ∈         |     | − = |       | ∈                        |     |     |
| --------------- | --------- | --- | --- | ----- | ------------------------ | --- | --- |
| 0 S. Similarly6 | S,since(6 |     | 3)  | 3and3 | S.Continuinginthisway,we |     |     |
canconcludethatallmultiplesof3areinS.
|                               |     |     |     |     | ∈              |     | (cid:3)= |
| ----------------------------- | --- | --- | --- | --- | -------------- | --- | -------- |
| Whataboutothernaturalnumbers? |     |     |     | Is1 | S? Weknowthat1 |     | 0,sothe  |
|                               |     |     |     |     | −              | =−  |          |
firstconditionisnotsatisfied. Furthermore,(1 3) 2,whichisnotanat-
uralnumberandthusisnotamemberof S. Thereforethesecondcondition
|     |     |     |     |     |     | (cid:3)∈ | (cid:3)∈ |
| --- | --- | --- | --- | --- | --- | -------- | -------- |
is not satisfied. Since 1 satisfies neither condition, 1 S. Similarly, 2 S.
What about 4? 4 ∈ S only if 1 ∈ S. But 1 (cid:3)∈ S, so 4 (cid:3)∈ S, as well. Similarly,
wecanconcludethatifnisanaturalnumberandisnotamultipleof3,then
(cid:3)∈
n S.
Fromthisargument,weconcludethat Sisthesetofnaturalnumbersthat
aremultiplesof3.
Wecanusethisdefinitiontowriteaproceduretodecidewhetheranatural
numbernisinS.
| in-S? : N     | → Bool |          |      |       |                 |     |     |
| ------------- | ------ | -------- | ---- | ----- | --------------- | --- | --- |
| usage: (in-S? | n)     | = #t     | if n | is in | S, #f otherwise |     |     |
| (define in-S? |        |          |      |       |                 |     |     |
| (lambda       | (n)    |          |      |       |                 |     |     |
| (if (zero?    |        | n) #t    |      |       |                 |     |     |
| (if           | (>= (- | n 3)     | 0)   |       |                 |     |     |
| (in-S?        |        | (- n 3)) |      |       |                 |     |     |
#f))))
Here we have written a recursive procedure in Scheme that follows the
definition. Thenotationin-S?: N → Boolisacomment,calledthecontractfor
thisprocedure.Itmeansthatin-S?isintendedtobeaprocedurethattakes
a natural number and produces a boolean. Such comments are helpful for
readingandwritingcode.
|     |     | ∈   |     |     |     | =   |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
Todetermine whether n S, we first askwhether n 0. If itis, then the
answeristrue. Otherwiseweneedtoseewhethern − 3 ∈ S. Todothis,we
− ≥
firstchecktoseewhether(n 3) 0. Ifitis,wethencanuseourprocedure
| toseewhetheritisinS. |     | Ifitisnot,thenncannotbein |     |     |     | S.  |     |
| -------------------- | --- | ------------------------- | --- | --- | --- | --- | --- |

1.1 RecursivelySpecifiedData 3
Hereisanalternativewayofwritingdownthedefinitionof S.
Definition1.1.2 Definetheset StobethesmallestsetcontainedinNandsatisfy-
ingthefollowingtwoproperties:
∈
1. 0 S,and
∈ + ∈
2. ifn S,thenn 3 S.
A “smallest set” is the one that satisfies properties 1 and 2 and that is a
subset of any other set satisfying properties 1 and 2. It is easy to see that
there can be only one such set: if S and S both satisfy properties 1 and
1 2
⊆ ⊆
2, and both are smallest, then S S (since S is smallest), and S S
1 2 1 2 1
=
(since S is smallest), hence S S . We need this extracondition, because
2 1 2
otherwisetherearemanysetsthatsatisfytheremainingtwoconditions(see
exercise1.3).
Hereisyetanotherwayofwritingthedefinition:
∈
0 S
∈
n S
+ ∈
(n 3) S
This is simply a shorthand notation for the precedingversion of the def-
inition. Each entry is called a rule of inference, or just a rule; the horizontal
line is read as an “if-then.” The part above the line is called the hypothesis
ortheantecedent;thepartbelowthelineiscalledtheconclusionortheconse-
quent. Whentherearetwoormorehypotheseslisted,theyareconnectedby
animplicit “and”(seedefinition 1.1.5). Arule with no hypotheses iscalled
anaxiom. Weoftenwriteanaxiomwithoutthehorizontalline,like
∈
0 S
The rules are interpreted as saying that a natural number n is in S if
∈
andonly if the statement “n S”canbe derivedfromthe axiomsbyusing
therulesofinferencefinitelymanytimes. Thisinterpretationautomatically
makesSthesmallestsetthatisclosedundertherules.
Thesedefinitionsallsaythesamething. Wecallthefirstversionatop-down
definition,thesecondversionabottom-updefinition,andthethirdversiona
rules-of-inferenceversion.

4 1 InductiveSetsofData
Letusseehowthisworksonsomeotherexamples.
Definition1.1.3(listofintegers,top-down) ASchemelistisalistofintegers
ifandonlyifeither
1. itistheemptylist,or
2. itisapairwhosecarisanintegerandwhosecdrisalistofintegers.
WeuseInttodenotethesetofallintegers,andList-of-Inttodenotetheset
oflistsofintegers.
Definition1.1.4(listofintegers,bottom-up) ThesetList-of-Intisthesmallest
setofSchemelistssatisfyingthefollowingtwoproperties:
()∈
1. List-of-Int,and
| ∈              | ∈                  | l)∈            |
| -------------- | ------------------ | -------------- |
| 2. ifn Intandl | List-of-Int,then(n | . List-of-Int. |
“.” cons
Here we use the infix to denote the result of the operation in
Scheme. The phrase (n . l) denotes a Scheme pair whose car is n and
whosecdrisl.
Definition1.1.5(listofintegers,rulesofinference)
∈
() List-of-Int
|     | ∈     | ∈             |
| --- | ----- | ------------- |
|     | n Int | l List-of-Int |
(n . l) ∈ List-of-Int
These three definitions areequivalent. We can show how to use them to
generatesomeelementsofList-of-Int.
1. ()isalistofintegers,becauseofproperty1ofdefinition1.1.4orthefirst
ruleofdefinition1.1.5.
| (14 . ())isalistofintegers, |     |                                      |
| --------------------------- | --- | ------------------------------------ |
| 2.                          |     | becauseofproperty2ofdefinition1.1.4, |
since14isanintegerand()isalistofintegers. Wecanalsowritethisas
aninstanceofthesecondruleforList-of-Int.
|     | 14∈ Int | ()∈ List-of-Int  |
| --- | ------- | ---------------- |
|     | (14 .   | ())∈ List-of-Int |

1.1 RecursivelySpecifiedData 5
3. (3 . (14 . ())) is a list of integers, because of property 2, since 3
is an integer and (14 . ()) is a list of integers. We can write this as
anotherinstanceofthesecondruleforList-of-Int.
|     |     | 3∈ Int | (14 . ())∈    | List-of-Int |
| --- | --- | ------ | ------------- | ----------- |
|     |     | (3     | . (14 . ()))∈ | List-of-Int |
4. (-7 . (3 . (14 . ()))) is a list of integers, because of property 2,
since -7 is a integer and (3 . (14 . ())) is a list of integers. Once
morewecanwritethisasaninstanceofthesecondruleforList-of-Int.
|     | -7∈ |       | ()))∈             |             |
| --- | --- | ----- | ----------------- | ----------- |
|     |     | Int   | (3 . (14 .        | List-of-Int |
|     |     | (-7 . | (3 . (14 . ())))∈ | List-of-Int |
5. Nothingisalistofintegersunlessitisbuiltinthisfashion.
(), (14), (3
| Converting | from  | dot notation                | to list notation, | we see that |
| ---------- | ----- | --------------------------- | ----------------- | ----------- |
| 14),and(-7 | 3 14) | areallmembersofList-of-Int. |                   |             |
Wecanalsocombinetherulestogetapictureoftheentirechainofreason-
ingthatshowsthat(-7 . (3 . (14 . ())))∈ List-of-Int. Thetree-like
picturebelowiscalledaderivationordeductiontree.
|     |     |     | 14∈ N      | () ∈ List-of-Int |
| --- | --- | --- | ---------- | ---------------- |
|     |     | 3∈  | (14 .      | ())∈             |
|     |     |     | N          | List-of-Int      |
|     | -7∈ |     | ()))∈      |                  |
|     |     | N   | (3 . (14 . | List-of-Int      |
())))∈
|     | (-7 | . (3 | . (14 . | List-of-Int |
| --- | --- | ---- | ------- | ----------- |
Exercise1.1 [ (cid:3) ] Writeinductivedefinitionsofthefollowingsets. Writeeachdefini-
tion in all three styles (top-down, bottom-up, and rules of inference). Using your
rules,showthederivationofsomesampleelementsofeachset.
1. {3n+2|n∈N}
{2n+3m+1|n,m∈N}
2.
{(n,2n+1)|n∈N}
3.
{(n,n2)|n∈N}Donotmentionsquaringinyourrules.
| 4.  |     |     |     | Asahint,rememberthe |
| --- | --- | --- | --- | ------------------- |
equation(n+1)2=n2+2n+1.
(cid:3)(cid:3)
Exercise1.2 [ ] Whatsetsaredefinedbythefollowingpairsofrules?Explainwhy.
(n,k)∈S
1. (0,1)∈S
(n+1,k+7)∈S

6 1 InductiveSetsofData
(n,k)∈S
2. (0,1)∈S
(n+1,2k)∈S
(n,i,j)∈S
3. (0,0,1)∈S
(n+1,j,i+j)∈S
(n,i,j)∈S
4. [ (cid:3)(cid:3)(cid:3) ] (0,1,0)∈S
(n+1,i+2,i+j)∈S
Exercise1.3 [ (cid:3) ]FindasetTofnaturalnumberssuchthat0∈T,andwhenevern∈T,
thenn+3∈T,butT(cid:3)=S,whereSisthesetdefinedindefinition1.1.2.
1.1.2 DefiningSetsUsingGrammars
The previous examples have been fairly straightforward, but it is easy to
imagine how the process of describing more complex data types becomes
quite cumbersome. To help with this, we show how to specify sets with
grammars.Grammarsaretypicallyusedtospecifysetsofstrings,butwecan
usethemtodefinesetsofvaluesaswell.
Forexample,wecandefinethesetList-of-Intbythegrammar
List-of-Int::=()
List-of-Int::=(Int . List-of-Int)
Here we have two rules corresponding to the two properties in defini-
tion 1.1.4 above. The first rule says that the empty list is in List-of-Int, and
the second says that if n is in Int and l is in List-of-Int, then (n . l) is in
List-of-Int. Thissetofrulesiscalledagrammar.
Letuslookatthepiecesofthisdefinition. Inthisdefinitionwehave
• NonterminalSymbols. Thesearethenamesofthesetsbeingdefined. In
thiscasethereisonlyonesuchset,butingeneral,theremightbeseveral
setsbeingdefined. Thesesetsaresometimescalledsyntacticcategories.
We will use the convention that nonterminals and sets have names that
arecapitalized,butwewilluselower-casenameswhenreferringtotheir
elementsinprose. Thisissimplerthanitsounds. Forexample,Expression
∈
isanonterminal,butwewillwritee Expressionor“eisanexpression.”
Another common convention, calledBackus-NaurFormor BNF,istosur-
(cid:7) (cid:8)
roundthewordwithanglebrackets,e.g. expression .
• Terminal Symbols. These are the characters in the external representa-
tion, inthiscase., (,and). Wetypicallywritetheseusingatypewriter
font,e.g.lambda.

1.1 RecursivelySpecifiedData 7
• Productions. Therulesarecalledproductions. Eachproductionhasaleft-
handside, whichis anonterminal symbol, and aright-hand side, which
consists of terminal and nonterminal symbols. The left- and right-hand
=
sidesareusuallyseparatedbythesymbol:: ,readisorcanbe. Theright-
hand side specifies a method for constructing members of the syntactic
categoryintermsofothersyntacticcategoriesandterminalsymbols, such
astheleftparenthesis,rightparenthesis,andtheperiod.
Oftensome syntactic categoriesmentioned inaproductionareleftunde-
finedwhentheirmeaningissufficientlyclearfromcontext,suchasInt.
Grammarsareoftenwrittenusingsomenotationalshortcuts. Itiscommon
toomittheleft-handsideofaproductionwhenitisthesameastheleft-hand
sideoftheprecedingproduction. Usingthisconventionourexamplewould
bewrittenas
List-of-Int::=()
::=(Int.List-of-Int)
One can also write a set of rules for a single syntactic category by writ-
=
ingtheleft-handsideand:: justonce,followedbyalltheright-handsides
|
separatedbythespecialsymbol“ ”(verticalbar,reador). Thegrammarfor
|
List-of-Intcouldbewrittenusing“ ”as
List-of-Int::=() | (Int . List-of-Int)
{...}∗
AnothershortcutistheKleenestar,expressedbythenotation . When
this appears in a right-hand side, it indicates a sequence of any number of
instancesofwhateverappearsbetweenthebraces.UsingtheKleenestar,the
definitionofList-of-Intissimply
List-of-Int::=({Int}∗)
Thisincludesthepossibilityofnoinstancesatall. Iftherearezeroinstances,
wegettheemptystring.
{...}+
A variant of the star notation is Kleene plus , which indicates a
+ ∗
sequenceofoneormoreinstances. Substituting for intheexampleabove
woulddefinethesyntacticcategoryofnon-emptylistsofintegers.
Still another variant of the star notation is the separated list notation. For
example,wewrite
{
Int
}∗(c)todenoteasequenceofanynumberofinstances
of the nonterminal Int, separated by the non-empty character sequence c.
Thisincludesthepossibilityofnoinstancesatall. Iftherearezeroinstances,
wegettheemptystring. Forexample,
{
Int
}∗(,)includesthestrings

8 1 InductiveSetsofData
8
14, 12
7, 3, 14, 16
and
{
Int
}∗(;)includesthestrings
8
14; 12
7; 3; 14; 16
Thesenotationalshortcutsarenotessential. Itisalwayspossibletorewrite
thegrammarwithoutthem.
Ifasetisspecifiedbyagrammar,asyntacticderivationmaybeusedtoshow
thatagivendatavalueisamemberoftheset. Suchaderivationstartswith
thenonterminalcorrespondingtotheset.Ateachstep,indicatedbyanarrow
⇒
,anonterminalisreplacedbytheright-handsideofacorrespondingrule,
orwithaknownmemberofitssyntacticclassiftheclasswasleftundefined.
Forexample,thepreviousdemonstrationthat(14 . ())isalistofintegers
maybeformalizedwiththesyntacticderivation
List-of-Int
⇒(Int.List-of-Int)
⇒(14.List-of-Int)
⇒(14.())
Theorderinwhichnonterminalsarereplaceddoesnotmatter. Thus,here
isanotherderivationof(14 . ()).
List-of-Int
⇒(Int.List-of-Int)
⇒(Int.())
⇒(14.())
(cid:3)
Exercise1.4 [ ]WriteaderivationfromList-of-Intto(-7 . (3 . (14 . ()))).
Letusconsiderthedefinitionsofsomeotherusefulsets.
1. Many symbol manipulation procedures are designed to operate on lists
thatcontainonlysymbolsandothersimilarlyrestrictedlists. Wecallthese
listss-lists,definedasfollows:
Definition1.1.6(s-list,s-exp)
S-list ::=({S-exp}∗)
S-exp::=Symbol | S-list

1.1 RecursivelySpecifiedData 9
Ans-listisalistofs-exps,andans-expiseitherans-listorasymbol. Here
aresomes-lists.
(a b c)
(an (((s-list)) (with () lots) ((of) nesting)))
We may occasionally use an expanded definition of s-list with integers
allowed,aswellassymbols.
2. A binary tree with numeric leavesand interior nodes labeled with sym-
bols may be represented using three-element lists for the interior nodes
bythegrammar:
Definition1.1.7(binarytree)
Bintree::=Int | (Symbol Bintree Bintree)
Herearesomeexamplesofsuchtrees:
1
2
(foo 1 2)
(bar 1 (foo 1 2))
(baz
(bar 1 (foo 1 2))
(biz 4 5))
3. The lambda calculus is a simple language that is often used to study the
theory of programming languages. This language consists only of vari-
able references, procedures that take a single argument, and procedure
calls. Wecandefineitwiththegrammar:
Definition1.1.8(lambdaexpression)
LcExp::=Identifier
::=(lambda (Identifier) LcExp)
::=(LcExp LcExp)
whereanidentifierisanysymbolotherthanlambda.

10 1 InductiveSetsofData
The identifier in the second production is the name of a variable in the
bodyofthelambdaexpression. Thisvariableiscalledtheboundvariable
oftheexpression,becauseitbindsorcapturesanyoccurrencesofthevari-
ableinthebody. Anyoccurrenceofthatvariableinthebodyreferstothis
one.
Toseehowthisworks,considerthelambdacalculusextendedwitharith-
meticoperators. Inthatlanguage,
(lambda (x) (+ x 5))
is an expression in which x is the bound variable. This expression
describesaprocedurethatadds5toitsargument. Therefore,in
((lambda (x) (+ x 5)) (- x 7))
the last occurrence of x does not refer to the x that is bound in the
lambdaexpression. Wediscussthisinsection1.2.4,whereweintroduce
occurs-free?.
This grammar defines the elements of LcExp as Scheme values, so it
becomeseasytowriteprogramsthatmanipulatethem.
Thesegrammarsaresaidtobecontext-freebecausearuledefiningagiven
syntacticcategorymaybeappliedinanycontextthatmakesreferencetothat
syntacticcategory.Sometimesthisisnotrestrictiveenough. Considerbinary
search trees. A node in a binary search tree is either empty or contains an
integerandtwosubtrees
Binary-search-tree::=() | (Int Binary-search-tree Binary-search-tree)
Thiscorrectlydescribesthestructureofeachnodebutignoresanimportant
factaboutbinarysearchtrees: allthekeysintheleftsubtreearelessthan(or
equalto)thekeyinthecurrentnode,andallthekeysintherightsubtreeare
greaterthanthekeyinthecurrentnode.
Becauseof this additionalconstraint, not everysyntactic derivationfrom
Binary-search-treeleadstoacorrectbinarysearchtree. Todeterminewhether
a particular production can be applied in a particular syntactic derivation,
we have to look at the context in which the production is applied. Such
constraintsarecalledcontext-sensitiveconstraintsorinvariants.

1.1 RecursivelySpecifiedData 11
Context-sensitiveconstraintsalsoarisewhenspecifyingthesyntaxofpro-
gramminglanguages. Forinstance,inmanylanguageseveryvariablemust
be declaredbefore it is used. This constraint on the use of variablesis sen-
sitive to the context of their use. Formal methods can be used to specify
context-sensitive constraints, but these methods are far more complicated
than the ones we consider in this chapter. In practice, the usual approach
is first to specify a context-free grammar. Context-sensitive constraints are
then added using other methods. We show an example of such techniques
inchapter7.
1.1.3 Induction
Having described sets inductively, we can use the inductive definitions in
two ways: to prove theorems about members of the set and to write pro-
gramsthat manipulate them. Here we presentan exampleof such a proof;
writingtheprogramsisthesubjectofthenextsection.
Theorem1.1.1 Lettbeabinarytree,asdefinedindefinition1.1.7. Thentcontains
anoddnumberofnodes.
Proof: Theproofisbyinductiononthesizeoft,wherewetakethesizeof
ttobethenumberofnodesint. Theinductionhypothesis,IH(k),isthatany
≤
treeofsize khasanoddnumberofnodes.Wefollowtheusualprescription
for aninductive proof: we first provethat IH(0)is true, and we then prove
that whenever k is an integer such that IH is true for k, then IH is true for
+
k 1also.
1. Therearenotreeswith0nodes,soIH(0)holdstrivially.
≤
2. Letk beanintegersuchthatIH(k)holds, thatis,anytreewith knodes
+
actually has an odd number of nodes. We need to show that IH(k 1)
≤ +
holds as well: that any tree with k 1 nodes has an odd number of
≤ +
nodes. Ifthas k 1nodes,thereareexactlytwopossibilitiesaccording
tothedefinitionofabinarytree:
(a) tcouldbeoftheformn,wherenisaninteger. Inthiscase,thasexactly
onenode,andoneisodd.
(b) tcouldbeoftheform(sym t t ),wheresym isasymbolandt and
1 2 1
≤
t aretrees. Nowt andt musthavefewernodesthant. Sincethas
2 1 2
+ ≤
k 1nodes,t andt musthave knodes. Thereforetheyarecovered
1 2

12 1 InductiveSetsofData
+
byIH(k),andtheymusteachhaveanoddnumberofnodes,say2n 1
1
+
and2n 1nodes,respectively.Hencethetotalnumberofnodesinthe
2
tree,countingthetwosubtreesandtheroot,is
+ + + + = + + +
(2n 1) (2n 1) 1 2(n n 1) 1
1 2 1 2
whichisonceagainodd.
+
This completes the proof of the claim that IH(k 1) holds and therefore
completestheinduction.
Thekeytotheproofisthatthesubstructuresofatreetarealwayssmaller
thantitself. Thispatternofproofiscalledstructuralinduction.
ProofbyStructuralInduction
Toprovethataproposition IH(s)istrueforallstructuress,provethefollow-
ing:
1. IH istrueonsimplestructures(thosewithoutsubstructures).
2. IfIHistrueonthesubstructuresofs,thenitistrueonsitself.
Exercise1.5 [ (cid:3)(cid:3) ]Provethatife∈LcExp,thentherearethesamenumberofleftand
rightparenthesesine.
1.2 DerivingRecursive Programs
Wehaveusedthemethodofinductivedefinitiontocharacterizecomplicated
sets. Wehaveseenthatwecananalyzeanelementofaninductivelydefined
settoseehowitisbuiltfromsmallerelementsoftheset. Wehaveusedthis
idea to write a procedure in-S? to decide whether a natural number is in
theset S. Wenowusethesameideatodefinemoregeneralproceduresthat
computeoninductivelydefinedsets.
Recursiveproceduresrelyonanimportantprinciple:
TheSmaller-SubproblemPrinciple
Ifwecanreduceaproblemtoasmallersubproblem,wecancalltheprocedure
thatsolvestheproblem tosolvethesubproblem.

| 1.2 DerivingRecursivePrograms |     |     |     |     | 13  |
| ----------------------------- | --- | --- | --- | --- | --- |
Thesolutionreturnedforthesubproblemmaythenbeusedtosolvetheorig-
inalproblem.Thisworksbecauseeachtimewecalltheprocedure,itiscalled
withasmallerproblem,untileventuallyitiscalledwithaproblemthatcan
besolveddirectly,withoutanothercalltoitself.
Weillustratethisideawithasequenceofexamples.
list-length
1.2.1
ThestandardSchemeprocedurelengthdeterminesthenumberofelements
inalist.
| > (length | ’(a b c)) |     |     |     |     |
| --------- | --------- | --- | --- | --- | --- |
3
| > (length | ’((x) ())) |     |     |     |     |
| --------- | ---------- | --- | --- | --- | --- |
2
list-length,
| Let us write | our own | procedure, called |     | that does | the same |
| ------------ | ------- | ----------------- | --- | --------- | -------- |
thing.
We begin by writing down the contract for the procedure. The contract
specifies the sets of possible arguments and possible return values for the
procedure. Thecontractalsomayincludetheintendedusageorbehaviorof
the procedure. This helps us keep track of our intentions both as we write
and afterwards. In code, this would be a comment; we typeset it for read-
ability.
| list-length | : List →     | Int             |     |     |     |
| ----------- | ------------ | --------------- | --- | --- | --- |
|             | (list-length | l) = the length | of  |     |     |
| usage:      |              |                 | l   |     |     |
| (define     | list-length  |                 |     |     |     |
| (lambda     | (lst)        |                 |     |     |     |
...))
Wecandefinethesetoflistsby
|     | List::=() | |            |         |     |     |
| --- | --------- | ------------ | ------- | --- | --- |
|     |           | (Schemevalue | . List) |     |     |
Thereforewe consider each possibility for a list. If the list is empty, then
itslengthis0.
| list-length | : List →     | Int             |      |     |     |
| ----------- | ------------ | --------------- | ---- | --- | --- |
| usage:      | (list-length | l) = the length | of l |     |     |
| (define     | list-length  |                 |      |     |     |
| (lambda     | (lst)        |                 |      |     |     |
| (if         | (null? lst)  |                 |      |     |     |
0
...)))

14 1 InductiveSetsofData
Ifalistisnon-empty,thenitslengthisonemorethanthelengthofitscdr.
Thisgivesusacompletedefinition.
: →
| list-length | List         | Int      |             |
| ----------- | ------------ | -------- | ----------- |
| usage:      | (list-length | l) = the | length of l |
| (define     | list-length  |          |             |
| (lambda     | (lst)        |          |             |
| (if         | (null? lst)  |          |             |
0
| (+  | 1 (list-length | (cdr | lst)))))) |
| --- | -------------- | ---- | --------- |
Wecanwatchlist-lengthcomputebyusingitsdefinition.
| (list-length | ’(a               | (b c) d))    |         |
| ------------ | ----------------- | ------------ | ------- |
| = (+ 1       | (list-length      | ’((b c)      | d)))    |
| = (+ 1       | (+ 1 (list-length | ’(d))))      |         |
| = (+ 1       | (+ 1 (+ 1         | (list-length | ’())))) |
| = (+ 1       | (+ 1 (+ 1         | 0)))         |         |
= 3
nth-element
1.2.2
ThestandardSchemeprocedurelist-reftakesalistlstandazero-based
indexnandreturnselementnumbernoflst.
| > (list-ref | ’(a b | c) 1) |     |
| ----------- | ----- | ----- | --- |
b
Let us write our own procedure, called nth-element, that does the same
thing.
AgainweusethedefinitionofListabove.
What should (nth-element lst n) return when lst is empty? In this
| case,(nth-element |     | n)isaskingforanelementofanemptylist,sowe |     |
| ----------------- | --- | ---------------------------------------- | --- |
lst
reportanerror.
Whatshould (nth-element lst n) returnwhenlst isnon-empty? The
| answerdependsonn. |     | Ifn = 0,theanswerissimplythecaroflst. |     |
| ----------------- | --- | ------------------------------------- | --- |
What should (nth-element lst n) return when lst is non-empty and
| (cid:3)= |     |     | −   |
| -------- | --- | --- | --- |
n 0? Inthiscase,theansweristhe(n 1)-stelementofthecdroflst. Since
n ∈ N andn (cid:3)= 0,weknowthatn − 1mustalsobein N,sowecanfindthe
−
(n 1)-stelementbyrecursivelycallingnth-element.

| 1.2 DerivingRecursivePrograms |     |     |     |     |     |     | 15  |
| ----------------------------- | --- | --- | --- | --- | --- | --- | --- |
Thisleadsustothedefinition
|             |                        |         | ×   | →         |                  |        |     |
| ----------- | ---------------------- | ------- | --- | --------- | ---------------- | ------ | --- |
| nth-element |                        | : List  | Int | SchemeVal |                  |        |     |
| usage:      | (nth-element           |         | lst | n) =      | the n-th element | of lst |     |
| (define     | nth-element            |         |     |           |                  |        |     |
| (lambda     |                        | (lst n) |     |           |                  |        |     |
|             | (if (null?             | lst)    |     |           |                  |        |     |
|             | (report-list-too-short |         |     |           | n)               |        |     |
|             | (if                    | (zero?  | n)  |           |                  |        |     |
(car lst)
|         | (nth-element          |              | (cdr | lst)  | (- n 1)))))) |            |     |
| ------- | --------------------- | ------------ | ---- | ----- | ------------ | ---------- | --- |
| (define | report-list-too-short |              |      |       |              |            |     |
| (lambda |                       | (n)          |      |       |              |            |     |
|         | (eopl:error           | ’nth-element |      |       |              |            |     |
|         | "List                 | too short    |      | by ~s | elements.~%" | (+ n 1)))) |     |
|         |                       |              |      |       | × →          |            |     |
Here the notation nth-element : List Int SchemeVal means that nth-
element is a procedure that takes two arguments, a list and an integer, and
returnsaSchemevalue. Thisisthesamenotationthatisusedinmathemat-
| icswhenwewrite                                           |     | f : A | × B → | C.  |     |     |     |
| -------------------------------------------------------- | --- | ----- | ----- | --- | --- | --- | --- |
| Theprocedurereport-list-too-shortreportsanerrorcondition |     |       |       |     |     |     | by  |
callingeopl:error. Theprocedureeopl:errorabortsthe computation.
Its first argument is a symbol that allows the error message to identify the
procedurethatcalledeopl:error.
Thesecondargumentisastringthatis
then printed in the error message. There must then be an additional argu-
mentforeachinstanceofthecharactersequence~sinthestring. Thevalues
of these arguments are printed in place of the corresponding ~s when the
string is printed. A ~% is treated as a new line. After the error message
procedureeopl:erroris
| is printed, | the | computation |     | is aborted. | This |     | not |
| ----------- | --- | ----------- | --- | ----------- | ---- | --- | --- |
partofstandardScheme,butmostimplementationsofSchemeprovidesuch
a facility. We use proceduresnamed report- to report errors in a similar
fashionthroughoutthebook.
Watchhownth-elementcomputesitsanswer:
| (nth-element   |     | ’(a | b c   | d e) | 3)  |     |     |
| -------------- | --- | --- | ----- | ---- | --- | --- | --- |
| = (nth-element |     |     | ’(b c | d e) | 2)  |     |     |
| = (nth-element |     |     | ’(c   | d e) | 1)  |     |     |
| = (nth-element |     |     | ’(d   | e)   | 0)  |     |     |
= d
Herenth-elementrecursonshorterandshorter lists, andonsmallerand
smallernumbers.

16 1 InductiveSetsofData
Iferrorcheckingwereomitted,wewouldhavetorelyoncarandcdrto
complainaboutbeingpassedtheemptylist,buttheirerrormessageswould
belesshelpful. Forexample,ifwereceivedanerrormessagefromcar,we
mighthavetolookforusesofcarthroughoutourprogram.
(cid:3)
Exercise1.6 [ ] Ifwereversedtheorderofthetestsinnth-element,whatwould
gowrong?
(cid:3)(cid:3)
Exercise1.7 [ ] Theerrormessagefromnth-elementisuninformative. Rewrite
nth-elementsothatitproducesamoreinformativeerrormessage,suchas“(a b
c)doesnothave8elements.”
1.2.3 remove-first
Theprocedureremove-firstshouldtaketwoarguments:asymbol,s,and
alistofsymbols,los. Itshouldreturnalistwiththesameelementsarranged
in the same order as los, except that the first occurrence of the symbol s is
removed.Ifthereisnooccurrenceofsinlos,thenlosisreturned.
> (remove-first ’a ’(a b c))
(b c)
> (remove-first ’b ’(e f g))
(e f g)
> (remove-first ’a4 ’(c1 a4 c1 a4))
(c1 c1 a4)
> (remove-first ’x ’())
()
Beforewestartwritingthedefinitionofthisprocedure,wemustcomplete
theproblemspecificationbydefiningthesetList-of-Symboloflistsofsymbols.
Unlikethes-listsintroducedinthelastsection,theselistsofsymbolsdonot
containsublists.
List-of-Symbol::=() | (Symbol . List-of-Symbol)
Alistofsymbols iseithertheemptylistor alistwhose carisasymboland
whosecdrisalistofsymbols.

1.2 DerivingRecursivePrograms 17
Ifthelistisempty,therearenooccurrencesofstoremove,sotheanswer
istheemptylist.
remove-first : Sym × Listof(Sym) → Listof(Sym)
usage: (remove-first s los) returns a list with
the same elements arranged in the same
order as los, except that the first
occurrence of the symbol s is removed.
(define remove-first
(lambda (s los)
(if (null? los)
’()
...)))
HerewehavewrittenthecontractwithListof(Sym)insteadofList-of-Symbol.
Thisnotationwillallowustoavoidmanydefinitionsliketheonesabove.
Iflosisnon-empty,istheresomecasewherewecandeterminetheanswer
immediately? Ifthefirstelementoflosiss,saylos =(ss 1 ... s n−1 ),thefirst
occurrenceofsisasthefirstelementoflos. Sotheresultofremovingitisjust
(s 1 ... s n−1 ).
remove-first : Sym × Listof(Sym) → Listof(Sym)
(define remove-first
(lambda (s los)
(if (null? los)
’()
(if (eqv? (car los) s)
(cdr los)
...))))
Ifthefirstelementoflosisnots,saylos =(s 0 s 1 ... s n−1 ),thenweknowthat
s is not the first occurrence of s. Therefore the first element of the answer
0
mustbe s , which isthe valueof the expression(car los). Furthermore,
0
thefirstoccurrenceofsinlosmustbeitsfirstoccurrencein(s 1 ... s n−1 ). So
therestoftheanswermustbetheresultofremovingthefirstoccurrenceof
s from the cdr of los. Since the cdr of los is shorter than los, we may recur-
sivelycallremove-firsttoremovesfromthecdroflos. Sothecdrofthe
answer canbe obtained asthe value of (remove-first s (cdr los)).
Since we know how to find the car and cdr of the answer, we can find the
whole answer by combining them with cons, using the expression (cons
(car los) (remove-first s (cdr los))). With this, the complete
definitionofremove-firstbecomes

18 1 InductiveSetsofData
|              |              | ×           | →           |     |     |
| ------------ | ------------ | ----------- | ----------- | --- | --- |
| remove-first | : Sym        | Listof(Sym) | Listof(Sym) |     |     |
| (define      | remove-first |             |             |     |     |
| (lambda      | (s           | los)        |             |     |     |
|              | (if (null?   | los)        |             |     |     |
’()
|     | (if (eqv? | (car los) | s)  |     |     |
| --- | --------- | --------- | --- | --- | --- |
(cdr los)
|     | (cons | (car los) | (remove-first | s (cdr los))))))) |     |
| --- | ----- | --------- | ------------- | ----------------- | --- |
Exercise1.8 [ (cid:3) ] In the definition of remove-first, if the last line were replaced
by(remove-first s (cdr los)),whatfunctionwouldtheresultingprocedure
compute?Givethecontract,includingtheusagestatement,fortherevisedprocedure.
(cid:3)(cid:3)
Exercise1.9 [ ] Define remove, which is like remove-first, except that it
removesalloccurrencesofagivensymbolfromalistofsymbols,notjustthefirst.
1.2.4 occurs-free?
Theprocedureoccurs-free?should takeavariablevar,representedasa
Schemesymbol, andalambda-calculusexpressionexp asdefinedindefini-
tion1.1.8,anddeterminewhetherornotvaroccursfreeinexp.Wesaythata
variableoccursfreeinanexpressionexp ifithassomeoccurrenceinexp that
isnotinsidesomelambdabindingofthesamevariable.Forexample,
| > (occurs-free? |     | ’x ’x) |     |     |     |
| --------------- | --- | ------ | --- | --- | --- |
#t
| > (occurs-free? |     | ’x ’y) |     |     |     |
| --------------- | --- | ------ | --- | --- | --- |
#f
| > (occurs-free? |     | ’x ’(lambda | (x) (x | y))) |     |
| --------------- | --- | ----------- | ------ | ---- | --- |
#f
| > (occurs-free? |     | ’x ’(lambda | (y) (x | y))) |     |
| --------------- | --- | ----------- | ------ | ---- | --- |
#t
| > (occurs-free? |     | ’x ’((lambda | (x) x) | (x y))) |     |
| --------------- | --- | ------------ | ------ | ------- | --- |
#t
| > (occurs-free? |     | ’x ’(lambda | (y) (lambda | (z) (x | (y z))))) |
| --------------- | --- | ----------- | ----------- | ------ | --------- |
#t
Wecansolvethisproblembyfollowingthegrammarforlambda-calculus
expressions
LcExp::=Identifier
|     |     | ::=(lambda | (Identifier) | LcExp) |     |
| --- | --- | ---------- | ------------ | ------ | --- |
::=(LcExp
LcExp)

1.2 DerivingRecursivePrograms 19
Wecansummarizethesecasesintherules:
• Iftheexpressioneisavariable,thenthevariablexoccursfreeineif
andonlyifxisthesamease.
• Iftheexpressione isofthe form(lambda (y) e (cid:12)), thenthevari-
ablexoccursfreeineifandonlyifyisdifferentfromxandxoccurs
(cid:12)
freeine.
• If the expression e is of the form (e e ), then x occurs freein e if
1 2
and only if it occurs free in e or e . Here, we use “or” to mean
1 2
inclusiveor, meaningthatthisincludesthe possibilitythat x occurs
freeinbothe ande . Wewillgenerallyuse“or”inthissense.
1 2
Youshouldconvinceyourselfthattheserulescapturethenotionofoccurring
“notinsidealambda-bindingofx.”
(cid:3)
Exercise1.10 [ ]Wetypicallyuse“or”tomean“inclusiveor.”Whatothermeanings
can“or”have?
Thenitiseasytodefineoccurs-free?. Sincetherearethreealternatives
to be checked, we use a Scheme cond rather than an if. In Scheme, (or
exp exp )returnsatruevalueifeitherexp orexp returnsatruevalue.
1 2 1 2
occurs-free? : Sym × LcExp → Bool
usage: returns #t if the symbol var occurs free
in exp, otherwise returns #f.
(define occurs-free?
(lambda (var exp)
(cond
((symbol? exp) (eqv? var exp))
((eqv? (car exp) ’lambda)
(and
(not (eqv? var (car (cadr exp))))
(occurs-free? var (caddr exp))))
(else
(or
(occurs-free? var (car exp))
(occurs-free? var (cadr exp)))))))
This procedure is not as readable as it might be. It is hard to tell, for
example, that (car (cadr exp)) refers to the declaration of a variable
ina lambdaexpression, or that(caddr exp)refersto itsbody. We show
howtoimprovethissituationconsiderablyinsection2.5.

20 1 InductiveSetsofData
subst
1.2.5
The proceduresubstshould takethreearguments: two symbols, newand
old, and an s-list, slist. All elements of slist are examined, and a
newlistisreturnedthatissimilartoslistbutwithalloccurrencesofold
replacedbyinstancesofnew.
| > (subst  | ’a ’b ’((b | c) (b () d))) |     |     |
| --------- | ---------- | ------------- | --- | --- |
| ((a c) (a | () d))     |               |     |     |
Sincesubstisdefinedovers-lists,itsorganizationshouldreflectthedefini-
tionofs-lists(definition1.1.6)
S-list ::=({S-exp}∗)
|     |     | S-exp::=Symbol | |   |     |
| --- | --- | -------------- | --- | --- |
S-list
The Kleene star gives a concise description of the set of s-lists, but it is
not so helpful for writing programs. Therefore our first step is to rewrite
thegrammartoeliminatetheuseoftheKleenestar. Theresultinggrammar
suggeststhatourprocedureshouldrecuronthecarandcdrofans-list.
S-list ::=()
|     |     | ::=(S-exp      | . S-list) |     |
| --- | --- | -------------- | --------- | --- |
|     |     | S-exp::=Symbol | |         |     |
S-list
| Thisexampleismorecomplexthanour |     |     | previousonesbecausethe | gram- |
| ------------------------------- | --- | --- | ---------------------- | ----- |
mar for its input contains two nonterminals, S-list and S-exp. Thereforewe
will have two procedures, one for dealing with S-list and one for dealing
withS-exp:
| subst : Sym | × Sym    | × S-list → S-list |     |     |
| ----------- | -------- | ----------------- | --- | --- |
| (define     | subst    |                   |     |     |
| (lambda     | (new old | slist)            |     |     |
...))
| subst-in-s-exp | : Sym          | × Sym × S-exp | → S-exp |     |
| -------------- | -------------- | ------------- | ------- | --- |
| (define        | subst-in-s-exp |               |         |     |
| (lambda        | (new old       | sexp)         |         |     |
...))

1.2 DerivingRecursivePrograms 21
Letusfirstworkonsubst. Ifthelistisempty,therearenooccurrencesof
oldtoreplace.
|         | :   | ×             | ×      | →      |     |
| ------- | --- | ------------- | ------ | ------ | --- |
| subst   | Sym | Sym           | S-list | S-list |     |
| (define |     | subst         |        |        |     |
| (lambda |     | (new old      | slist) |        |     |
|         | (if | (null? slist) |        |        |     |
’()
...)))
Ifslistisnon-empty,itscarisamemberofS-expanditscdrisanothers-list.
In this case, the answer should be a list whose car is the result of changing
oldtonewinthecarofslist,andwhosecdristheresultofchangingold
tonewinthecdrofslist. SincethecarofslistisanelementofS-exp,we
solve the subproblemforthe carusing subst-in-s-exp. Since the cdrof
slistisanelementofS-list,werecuronthecdrusingsubst:
| subst   | : Sym | × Sym         | × S-list | → S-list |     |
| ------- | ----- | ------------- | -------- | -------- | --- |
| (define |       | subst         |          |          |     |
| (lambda |       | (new old      | slist)   |          |     |
|         | (if   | (null? slist) |          |          |     |
’()
(cons
|     |     | (subst-in-s-exp |     | new old          | (car slist)) |
| --- | --- | --------------- | --- | ---------------- | ------------ |
|     |     | (subst new      | old | (cdr slist)))))) |              |
Now we canmove on to subst-in-s-exp. From the grammar, we know
that the symbol expression sexp is either a symbol or an s-list. If it is a
symbol,weneedtoaskwhetheritisthesameasthesymbolold. Ifitis,the
answerisnew;ifitissomeothersymbol,theansweristhesameassexp. If
sexpisans-list,thenwecanrecurusingsubsttofindtheanswer.
| subst-in-s-exp |        | : Sym          | × Sym    | × S-exp   | → S-exp |
| -------------- | ------ | -------------- | -------- | --------- | ------- |
| (define        |        | subst-in-s-exp |          |           |         |
| (lambda        |        | (new old       | sexp)    |           |         |
|                | (if    | (symbol? sexp) |          |           |         |
|                | (if    | (eqv? sexp     | old)     | new sexp) |         |
|                | (subst | new old        | sexp)))) |           |         |
Since we havestrictly followed the definition of S-list and S-exp, this recur-
sion is guaranteed to halt. Since subst and subst-in-s-exp call each
otherrecursively,wesaytheyaremutuallyrecursive.
The decomposition of substinto two procedures, one for eachsyntactic
category, is an important technique. It allows us to think about one syn-
tactic category at a time, which greatly simplifies our thinking about more
complicatedprograms.

22 1 InductiveSetsofData
(cid:3)
Exercise1.11 [ ] Inthelastlineofsubst-in-s-exp,therecursionisonsexpand
notasmallersubstructure.Whyistherecursionguaranteedtohalt?
(cid:3)
Exercise1.12 [ ]Eliminatetheonecalltosubst-in-s-expinsubstbyreplacingit
byitsdefinitionandsimplifyingtheresultingprocedure.Theresultwillbeaversion
ofsubstthatdoesnotneedsubst-in-s-exp.Thistechniqueiscalledinlining,and
isusedbyoptimizingcompilers.
(cid:3)(cid:3)
Exercise1.13 [ ] In our example, we began by eliminating the Kleene star inthe
grammarforS-list.Writesubstfollowingtheoriginalgrammarbyusingmap.
We’ve now developed a recipe for writing procedures that operate on
inductivelydefineddatasets. Wesummarizeitasaslogan.
FollowtheGrammar!
When defining a procedure that operates on inductively defined data, the
structureoftheprogram shouldbepatterned afterthestructureofthedata.
Moreprecisely:
• Writeoneprocedureforeachnonterminalinthegrammar.Theprocedure
willberesponsibleforhandlingthedatacorrespondingtothatnontermi-
nal,andnothingelse.
• Ineachprocedure,writeonealternativeforeachproductioncorrespond-
ingtothatnonterminal. Youmayneedadditionalcasestructure,butthis
willgetyoustarted. Foreachnonterminalthatappearsintheright-hand
side,writearecursivecalltotheprocedureforthatnonterminal.
1.3 AuxiliaryProceduresandContextArguments
The Follow-the-Grammar recipe is powerful, but sometimes it is not suffi-
cient. Consider the procedurenumber-elements. This procedureshould
takeanylist(v v v ...) andreturnthelist((0 v ) (1v ) (2 v )
0 1 2 0 1 2
...).
A straightforward decomposition of the kind we’ve used so far does not
solve this problem, because there is no obvious way to build the value of
(number-elements lst) from the value of (number-elements (cdr
lst))(butseeexercise1.36).
Tosolvethisproblem,weneedtogeneralizetheproblem. Wewriteanew
procedure number-elements-from that takes an additional argument n

1.3 AuxiliaryProceduresandContextArguments 23
that specifies the number to start from. This procedureis easy to write, by
recursiononthelist.
number-elements-from : Listof(SchemeVal) × Int → Listof(List(Int,SchemeVal))
usage: (number-elements-from ’(v v v ...) n)
0 1 2
= ((n v ) (n+1 v ) (n+2 v ) ...)
0 1 2
(define number-elements-from
(lambda (lst n)
(if (null? lst) ’()
(cons
(list n (car lst))
(number-elements-from (cdr lst) (+ n 1))))))
Herethecontractheadertellsusthatthisproceduretakestwoarguments,
a list (containing any Scheme values) and an integer, and returns a list of
things, each of which is a list consisting of two elements: an integer and a
Schemevalue.
Once we have defined number-elements-from, it’s easy to write the
desiredprocedure.
number-elements : List → Listof(List(Int,SchemeVal))
(define number-elements
(lambda (lst)
(number-elements-from lst 0)))
There are two important observations to be made here. First, the proce-
durenumber-elements-fromhasaspecificationthatisindependentofthe
specification of number-elements. It’s very common for a programmer
to write a procedurethat simply calls some auxiliary procedurewith some
additionalconstantarguments. Unless we canunderstandwhat thatauxil-
iaryproceduredoesforeveryvalueofitsarguments,thenwecan’tpossibly
understandwhatthecallingproceduredoes. Thisgivesusaslogan:
NoMysteriousAuxiliaries!
When defining an auxiliary procedure, always specify what it does on all
arguments,notjusttheinitialvalues.
Second,thetwoargumentstonumber-elements-fromplaytwodiffer-
ent roles. The first argument is the list we are working on. It gets smaller
at every recursive call. The second argument, however, is an abstraction

24 1 InductiveSetsofData
of the context in which we are working. In this example, when we call
number-elements,weend upcallingnumber-elements-fromon each
sublist of the original list. The second argument tells us the position of the
sublistintheoriginallist. Thisneednotdecreaseatarecursivecall;indeedit
grows,becausewearepassingoveranotherelementoftheoriginallist. We
sometimescallthisacontextargumentorinheritedattribute.
Asanotherexample,considertheproblemofsummingallthevaluesina
vector.
Ifweweresummingthevaluesinalist, wecouldfollowthegrammarto
recuronthecdrofthelist. Thiswouldgetusaprocedurelike
list-sum : Listof(Int) → Int
(define list-sum
(lambda (loi)
(if (null? loi)
0
(+ (car loi)
(list-sum (cdr loi))))))
Butitisnotpossibletoproceedinthiswaywithvectors,becausetheydonot
decomposeasreadily.
Since we cannot decompose vectors, we generalize the problem to com-
pute the sum of part of the vector. The specification of our problem is to
compute
i=length(v)−1
∑
v
i
i=0
wherevisavectorofintegers. Wegeneralizeitbyturningtheupperbound
intoaparametern,sothatthenewtaskistocompute
i=n
∑
v
i
i=0
≤ <
where0 n length(v).
This procedure is straightforward to write from its specification, using
inductiononitssecondargumentn.

1.4 Exercises 25
|                    |      |                 |            | × →     |
| ------------------ | ---- | --------------- | ---------- | ------- |
| partial-vector-sum |      | : Vectorof(Int) |            | Int Int |
|                    |      | ≤ <             |            |         |
| usage:             | if 0 | n               | length(v), | then    |
i∑ =n
|     |     | (partial-vector-sum |     | n) = |
| --- | --- | ------------------- | --- | ---- |
v v i
i=0
| (define | partial-vector-sum  |     |      |                |
| ------- | ------------------- | --- | ---- | -------------- |
| (lambda | (v                  | n)  |      |                |
| (if     | (zero?              | n)  |      |                |
|         | (vector-ref         |     | v 0) |                |
|         | (+ (vector-ref      |     | v n) |                |
|         | (partial-vector-sum |     |      | v (- n 1)))))) |
Since n decreases steadily to zero, a proof of correctness for this pro-
gram would proceed by induction on n. Because 0 ≤ n and n (cid:3)= 0, we
|     |     | ≤   | −   |     |
| --- | --- | --- | --- | --- |
can deduce that 0 (n 1), so that the recursive call to the procedure
partial-vector-sum
satisfiesitscontract.
It is now a simple matter to solve our original problem. The procedure
partial-vector-sumdoesn’tapplyifthevectorisoflength0,soweneed
tohandlethatcaseseparately.
| vector-sum | :   | Vectorof(Int) | → Int |     |
| ---------- | --- | ------------- | ----- | --- |
i=length(v)−1
∑
| usage: | (vector-sum |     | v) = | v   |
| ------ | ----------- | --- | ---- | --- |
i
i=0
| (define | vector-sum |                |     |      |
| ------- | ---------- | -------------- | --- | ---- |
| (lambda | (v)        |                |     |      |
| (let    | ((n        | (vector-length |     | v))) |
|         | (if (zero? | n)             |     |      |
0
|     | (partial-vector-sum |     |     | v (- n 1)))))) |
| --- | ------------------- | --- | --- | -------------- |
There are many other situations in which it may be helpful or necessary
to introduce auxiliary variables or procedures to solve a problem. Always
feelfreetodoso,providedthatyoucangiveanindependentspecificationof
whatthenewprocedureisintendedtodo.
Exercise1.14 [ (cid:3)(cid:3) ] Given the assumption 0≤ n <length(v), prove that partial-
vector-sumiscorrect.
1.4 Exercises
Gettingtheknackofwritingrecursiveprogramsinvolvespractice. Thuswe
concludethischapterwithasequenceofexercises.
s n
In each of these exercises, assume that is a symbol, is a nonnegative
integer,lstisalist,loiisalistofintegers,losisalistofsymbols,slist

26 1 InductiveSetsofData
isans-list,andxisanySchemevalue;andsimilarlys1isasymbol,los2is
alistofsymbols,x1isaSchemevalue,etc. Alsoassumethatpredisapred-
icate, that is, a procedure that takes any Scheme value and always returns
either #t or #f. Make no other assumptions about the data unless further
restrictions are given as part of a particular problem. For these exercises,
there is no need to check that the input matches the description; for each
procedure,assumethatitsinputvaluesaremembersofthespecifiedsets.
Define, test,anddebugeachprocedure. Your definitionshould include a
contractandusagecomment inthe stylewe haveusedinthischapter. Feel
freetodefineauxiliaryprocedures,buteachauxiliaryprocedureyoudefine
shouldhaveitsownspecification,asinsection1.3.
To test these procedures, first try all the given examples. Then use other
examplestotesttheseprocedures,sincethegivenexamplesarenotadequate
torevealallpossibleerrors.
(cid:3)
| Exercise1.15 | [   | ](duple | n x)returnsalistcontainingncopiesofx. |     |     |
| ------------ | --- | ------- | ------------------------------------- | --- | --- |
| > (duple     | 2   | 3)      |                                       |     |     |
(3 3)
| > (duple | 4       | ’(ha     | ha))    |          |     |
| -------- | ------- | -------- | ------- | -------- | --- |
| ((ha     | ha) (ha | ha)      | (ha ha) | (ha ha)) |     |
| > (duple | 0       | ’(blah)) |         |          |     |
()
Exercise1.16 [ (cid:3) ](invert lst),wherelstisalistof2-lists(listsoflengthtwo),
returnsalistwitheach2-listreversed.
| > (invert |     | ’((a  | 1) (a 2) | (1 b) | (2 b))) |
| --------- | --- | ----- | -------- | ----- | ------- |
| ((1 a)    | (2  | a) (b | 1) (b    | 2))   |         |
(cid:3)
Exercise1.17 [ ](down lst)wrapsparenthesesaroundeachtop-levelelementof
lst.
| > (down | ’(1      | 2 3))           |                |           |          |
| ------- | -------- | --------------- | -------------- | --------- | -------- |
| ((1)    | (2) (3)) |                 |                |           |          |
| > (down | ’((a)    | (fine)          | (idea)))       |           |          |
| (((a))  | ((fine)) |                 | ((idea)))      |           |          |
| > (down | ’(a      | (more           | (complicated)) |           | object)) |
| ((a)    | ((more   | (complicated))) |                | (object)) |          |

1.4 Exercises 27
(cid:3)
Exercise1.18 [ ](swapper s1 s2 slist)returnsalistthe same asslist, but
withalloccurrencesofs1replacedbys2andalloccurrencesofs2replacedbys1.
| > (swapper | ’a ’d ’(a | b   | c d)) |     |     |
| ---------- | --------- | --- | ----- | --- | --- |
(d b c a)
| > (swapper | ’a ’d ’(a   | d   | () c d))    |     |     |
| ---------- | ----------- | --- | ----------- | --- | --- |
| (d a ()    | c a)        |     |             |     |     |
| > (swapper | ’x ’y ’((x) |     | y (z (x)))) |     |     |
| ((y) x     | (z (y)))    |     |             |     |     |
(cid:3)(cid:3)
Exercise1.19 [ ](list-set lst n x)returnsalistlikelst,exceptthatthen-th
element,usingzero-basedindexing,isx.
| > (list-set | ’(a b     | c d) | 2 ’(1 | 2))      |           |
| ----------- | --------- | ---- | ----- | -------- | --------- |
| (a b (1     | 2) d)     |      |       |          |           |
| > (list-ref | (list-set | ’(a  | b c   | d) 3 ’(1 | 5 10)) 3) |
(1 5 10)
(cid:3)
Exercise1.20 [ ] (count-occurrences s slist) returns the number of occur-
rencesofsinslist.
| > (count-occurrences |     | ’x  | ’((f | x) y (((x | z) x)))) |
| -------------------- | --- | --- | ---- | --------- | -------- |
3
| > (count-occurrences |     | ’x  | ’((f | x) y (((x | z) () x)))) |
| -------------------- | --- | --- | ---- | --------- | ----------- |
3
| > (count-occurrences |     | ’w  | ’((f | x) y (((x | z) x)))) |
| -------------------- | --- | --- | ---- | --------- | -------- |
0
(cid:3)(cid:3)
Exercise1.21 [ ] (product sos1 sos2), where sos1 and sos2 are each a list
ofsymbolswithoutrepetitions, returnsalistof2-liststhat representsthe Cartesian
productofsos1andsos2.The2-listsmayappearinanyorder.
| > (product | ’(a b c)    | ’(x   | y))   |        |     |
| ---------- | ----------- | ----- | ----- | ------ | --- |
| ((a x)     | (a y) (b x) | (b y) | (c x) | (c y)) |     |
(cid:3)(cid:3)
Exercise1.22 [ ] (filter-in pred lst) returns the list of those elements in
lstthatsatisfythepredicatepred.
| > (filter-in | number? | ’(a | 2 (1 | 3) b 7)) |     |
| ------------ | ------- | --- | ---- | -------- | --- |
(2 7)
| > (filter-in | symbol? | ’(a | (b c) | 17 foo)) |     |
| ------------ | ------- | --- | ----- | -------- | --- |
(a foo)
(cid:3)(cid:3)
Exercise1.23 [ ] (list-index pred lst) returns the 0-based position of the
firstelementoflstthatsatisfiesthepredicatepred. Ifnoelementoflstsatisfies
thepredicate,thenlist-indexreturns#f.
| > (list-index | number? | ’(a | 2 (1 | 3) b 7)) |     |
| ------------- | ------- | --- | ---- | -------- | --- |
1
| > (list-index | symbol? | ’(a | (b  | c) 17 foo)) |     |
| ------------- | ------- | --- | --- | ----------- | --- |
0
| > (list-index | symbol? | ’(1 | 2 (a | b) 3)) |     |
| ------------- | ------- | --- | ---- | ------ | --- |
#f

| 28  |     |     |     |     | 1 InductiveSetsofData |     |
| --- | --- | --- | --- | --- | --------------------- | --- |
(cid:3)(cid:3)
Exercise1.24 [ ] (every? pred lst) returns #f if any element of lst fails to
| satisfypred,andreturns#t |         |     | otherwise. |         |     |     |
| ------------------------ | ------- | --- | ---------- | ------- | --- | --- |
| > (every?                | number? |     | ’(a b      | c 3 e)) |     |     |
#f
| > (every? | number? |     | ’(1 2 | 3 5 4)) |     |     |
| --------- | ------- | --- | ----- | ------- | --- | --- |
#t
(cid:3)(cid:3)
Exercise1.25 [ ](exists? pred lst)returns#tifanyelementoflstsatisfies
| pred,andreturns#f |     | otherwise. |       |         |     |     |
| ----------------- | --- | ---------- | ----- | ------- | --- | --- |
| > (exists?        |     | number?    | ’(a b | c 3 e)) |     |     |
#t
| > (exists? |     | number? | ’(a b | c d e)) |     |     |
| ---------- | --- | ------- | ----- | ------- | --- | --- |
#f
(cid:3)(cid:3)
Exercise1.26 [ ](up lst)removesapairofparenthesesfromeachtop-levelele-
ment of lst. If a top-level element is not a list, it is included in the result, as is.
The value of (up (down lst)) is equivalent to lst, but (down (up lst)) is
notnecessarilylst.(Seeexercise1.17.)
| > (up        | ’((1           | 2) (3 4))) |        |         |                       |              |
| ------------ | -------------- | ---------- | ------ | ------- | --------------------- | ------------ |
| (1 2         | 3 4)           |            |        |         |                       |              |
| > (up        | ’((x           | (y)) z))   |        |         |                       |              |
| (x (y)       | z)             |            |        |         |                       |              |
|              | (cid:3)(cid:3) | (flatten   | slist) |         |                       |              |
| Exercise1.27 | [              | ]          |        | returns | a list of the symbols | contained in |
slistintheorderinwhichtheyoccurwhenslistisprinted.Intuitively,flatten
removesalltheinnerparenthesesfromitsargument.
| > (flatten |     | ’(a b c)) |     |     |     |     |
| ---------- | --- | --------- | --- | --- | --- | --- |
(a b c)
| > (flatten |     | ’((a) () | (b ()) | () (c))) |     |     |
| ---------- | --- | -------- | ------ | -------- | --- | --- |
(a b c)
| > (flatten |        | ’((a b)   | c (((d)) | e))) |     |     |
| ---------- | ------ | --------- | -------- | ---- | --- | --- |
| (a b       | c d e) |           |          |      |     |     |
| > (flatten |        | ’(a b (() | (c))))   |      |     |     |
(a b c)
Exercise1.28 [ (cid:3)(cid:3) ](merge loi1 loi2),whereloi1andloi2arelistsofintegers
thataresortedinascendingorder,returnsasortedlistofalltheintegersinloi1and
loi2.
| > (merge | ’(1    | 4) ’(1 | 2 8))  |        |         |     |
| -------- | ------ | ------ | ------ | ------ | ------- | --- |
| (1 1     | 2 4 8) |        |        |        |         |     |
| > (merge | ’(35   | 62 81  | 90 91) | ’(3 83 | 85 90)) |     |
| (3 35    | 62 81  | 83 85  | 90 90  | 91)    |         |     |

1.4 Exercises 29
(cid:3)(cid:3)
Exercise1.29 [ ] (sort loi) returns a list of the elements of loi in ascending
order.
| > (sort ’(8 2 5 | 2 3)) |     |
| --------------- | ----- | --- |
(2 2 3 5 8)
(cid:3)(cid:3)
Exercise1.30 [ ](sort/predicate pred loi)returnsalistofelementssorted
bythepredicate.
| > (sort/predicate | < ’(8 2 | 5 2 3)) |
| ----------------- | ------- | ------- |
(2 2 3 5 8)
| > (sort/predicate | > ’(8 2 | 5 2 3)) |
| ----------------- | ------- | ------- |
(8 5 3 2 2)
(cid:3)
Exercise1.31 [ ] Write the following procedures for calculating on a bintree (defi-
nition1.1.7): leafandinterior-node,whichbuildbintrees,leaf?,whichtests
whether abintreeisaleaf, and lson, rson, and contents-of, which extractthe
components of a node. contents-of should work on both leaves and interior
nodes.
(cid:3)
Exercise1.32 [ ]Writeaproceduredouble-treethattakesabintree,asrepresented
in definition 1.1.7, and produces another bintree like the original, but with all the
integersintheleavesdoubled.
(cid:3)(cid:3) ]Writeaproceduremark-leaves-with-red-depththattakesa
Exercise1.33 [
bintree (definition 1.1.7), and producesa bintree of the same shape as the original,
exceptthatinthenewtree,eachleafcontainstheintegerofnodesbetweenitandthe
rootthatcontainthesymbolred.Forexample,theexpression
(mark-leaves-with-red-depth
| (interior-node | ’red  |     |
| -------------- | ----- | --- |
| (interior-node | ’bar  |     |
| (leaf 26)      |       |     |
| (leaf 12))     |       |     |
| (interior-node | ’red  |     |
| (leaf 11)      |       |     |
| (interior-node | ’quux |     |
(leaf 117)
(leaf 14))
whichiswrittenusingtheproceduresdefinedinexercise1.31,shouldreturnthebin-
tree
(red
(bar 1 1)
| (red 2 (quux | 2 2))) |     |
| ------------ | ------ | --- |

30 1 InductiveSetsofData
(cid:3)(cid:3)(cid:3)
Exercise1.34 [ ] Write a procedure path that takes an integer n and a binary
searchtreebst(page10)thatcontainstheintegern,andreturnsalistofleftsand
rightsshowinghowtofindthenodecontainingn.Ifnisfoundattheroot,itreturns
theemptylist.
| > (path | 17 ’(14 (7 | () (12 () ())) |     |
| ------- | ---------- | -------------- | --- |
|         | (26        | (20 (17 () ()) |     |
())
(31 () ()))))
| (right left | left) |     |     |
| ----------- | ----- | --- | --- |
Exercise1.35 [ (cid:3)(cid:3)(cid:3) ] Write a procedure number-leaves that takes a bintree, and
producesabintreeliketheoriginal,exceptthecontents oftheleavesarenumbered
startingfrom0.Forexample,
(number-leaves
| (interior-node | ’foo |       |     |
| -------------- | ---- | ----- | --- |
| (interior-node |      | ’bar  |     |
| (leaf          | 26)  |       |     |
| (leaf          | 12)) |       |     |
| (interior-node |      | ’baz  |     |
| (leaf          | 11)  |       |     |
| (interior-node |      | ’quux |     |
(leaf 117)
(leaf 14))
shouldreturn
(foo
| (bar 0 | 1)  |     |     |
| ------ | --- | --- | --- |
(baz
2
| (quux | 3 4))) |     |     |
| ----- | ------ | --- | --- |
(cid:3)(cid:3)(cid:3)
Exercise1.36 [ ]Writeaproceduregsuchthatnumber-elementsfrompage23
couldbedefinedas
| (define | number-elements |                        |                |
| ------- | --------------- | ---------------------- | -------------- |
| (lambda | (lst)           |                        |                |
| (if     | (null? lst)     | ’()                    |                |
| (g      | (list 0 (car    | lst)) (number-elements | (cdr lst)))))) |

2
Data Abstraction
2.1 SpecifyingData via Interfaces
Everytime we decide to representa certain set of quantities in a particular
way,wearedefininganewdatatype: thedatatypewhosevaluesarethose
representations and whose operations are the procedures that manipulate
thoseentities.
Therepresentationoftheseentitiesisoftencomplex,sowedonotwantto
beconcernedwiththeirdetailswhenwecanavoidthem.Wemayalsodecide
tochangetherepresentationofthedata. Themostefficientrepresentationis
oftenalotmoredifficulttoimplement,sowemaywishtodevelopasimple
implementation first and only change to a more efficient representation if
it proves critical to the overall performance of a system. If we decide to
change the representation of some data for any reason, we must be able to
locateallpartsofaprogramthataredependentontherepresentation. This
isaccomplishedusingthetechniqueofdataabstraction.
Data abstraction divides a data type into two pieces: an interface and an
implementation. The interface tells us what the data of the type represents,
what the operations on the data are, and what properties these operations
may be relied on to have. The implementation provides a specific represen-
tation of the data and code for the operations that make use of that data
representation.
Adatatypethatisabstractinthiswayissaidtobeanabstractdatatype.The
restoftheprogram,theclientofthedatatype,manipulatesthenewdataonly
throughtheoperationsspecifiedintheinterface. Thusifwewishtochange
therepresentationofthedata,allwemustdoischangetheimplementation
oftheoperationsintheinterface.

32 2 DataAbstraction
Thisisafamiliaridea:whenwewriteprogramsthatmanipulatefiles,most
of the time we care only that we can invoke procedures that perform the
open,close,read,andothertypicaloperationsonfiles. Similarly,mostofthe
time,wedon’tcarehowintegersareactuallyrepresentedinsidethemachine.
Ouronlyconcernisthatwecanperformthearithmeticoperationsreliably.
When the client manipulates the values of the data type only through
the proceduresin the interface, we saythat the client code is representation-
independent,becausethenthecodedoesnotrelyontherepresentationofthe
valuesinthedatatype.
Alltheknowledgeabouthowthedataisrepresentedmustthereforereside
inthecodeoftheimplementation. Themostimportantpartofanimplemen-
tationisthespecificationofhowthedataisrepresented.Weusethenotation
(cid:13) (cid:14)
v for“therepresentationofdatav.”
To make this clearer, let us consider a simple example: the data type of
naturalnumbers. The datato be representedare the naturalnumbers. The
interfaceistoconsistoffourprocedures:zero,is-zero?,successor,and
predecessor. Of course, notjustanysetof procedureswill beacceptable
asanimplementationofthisinterface.Asetofprocedureswillbeacceptable
as implementations of zero, is-zero?, successor, and predecessor
onlyiftheysatisfythefourequations
(zero)=(cid:13)
0
(cid:14)
(cid:2)
#t n = 0
(is-zero?(cid:13)
n
(cid:14))=
#f n (cid:3)= 0
(successor(cid:13)
n
(cid:14))=(cid:13)
n
+
1
(cid:14)
(n
≥
0)
(predecessor(cid:13)
n
+
1
(cid:14))=(cid:13)
n
(cid:14)
(n
≥
0)
This specification does not dictate how these natural numbers are to be
represented. It requiresonly that these proceduresconspire to produce the
specified behavior. Thus, the procedure zero must return the representa-
tionof 0. The proceduresuccessor, given the representationof the num-
+
ber n, must return the representation of the number n 1, and so on. The
specification says nothing about (predecessor (zero)), so under this
specificationanybehaviorwouldbeacceptable.

2.1 SpecifyingDataviaInterfaces 33
Wecannowwriteclientprogramsthatmanipulatenaturalnumbers, and
weareguaranteedthattheywillgetcorrectanswers,nomatterwhatrepre-
| sentationisinuse. | Forexample, |     |     |     |
| ----------------- | ----------- | --- | --- | --- |
(define plus
| (lambda (x    | y)  |     |     |     |
| ------------- | --- | --- | --- | --- |
| (if (is-zero? |     | x)  |     |     |
y
| (successor |     | (plus | (predecessor | x) y))))) |
| ---------- | --- | ----- | ------------ | --------- |
will satisfy (plus (cid:13) x (cid:14) (cid:13) y (cid:14))=(cid:13) x + y (cid:14) , no matter what implementation of
thenaturalnumbersweuse.
Most interfaces will contain some constructors that build elements of
the data type, and some observers that extract information from values of
the data type. Here we have three constructors, zero, successor, and
predecessor,andoneobserver,is-zero?.
Therearemanypossiblerepresentationsofthisinterface. Letusconsider
threeofthem.
1. Unary representation: In the unary representation, the natural number n
is representedby a list of n #t’s. Thus, 0 is represented by (), 1 is rep-
resented by (#t), 2 is represented by (#t #t), etc. We can define this
representationinductivelyby:
(cid:13) 0 (cid:14)=()
| (cid:13) + (cid:14)=(#t | (cid:13) | (cid:14)) |     |     |
| ----------------------- | -------- | --------- | --- | --- |
| n 1                     | .        | n         |     |     |
Inthisrepresentation,wecansatisfythespecificationbywriting
| (define zero        | (lambda |         | () ’())) |                   |
| ------------------- | ------- | ------- | -------- | ----------------- |
| (define is-zero?    |         | (lambda | (n)      | (null? n)))       |
| (define successor   |         | (lambda |          | (n) (cons #t n))) |
| (define predecessor |         | (lambda |          | (n) (cdr n)))     |
2. Scheme number representation: In this representation, we simply use
Scheme’sinternalrepresentationofnumbers(whichmightitselfbequite
(cid:13) (cid:14) n,
complicated!). We let n be the Scheme integer and define the four
requiredentitiesby
| (define zero        | (lambda |         | () 0)) |               |
| ------------------- | ------- | ------- | ------ | ------------- |
| (define is-zero?    |         | (lambda | (n)    | (zero? n)))   |
| (define successor   |         | (lambda |        | (n) (+ n 1))) |
| (define predecessor |         | (lambda |        | (n) (- n 1))) |

34 2 DataAbstraction
3. Bignum representation: In the bignum representation, numbers are repre-
sented in base N, for some large integer N. The representationbecomes
−
| alistconsistingofnumbersbetween0andN |     |     | 1(sometimescalledbigits |
| ------------------------------------ | --- | --- | ----------------------- |
ratherthandigits). Thisrepresentationmakesiteasytorepresentintegers
thataremuchlargerthancanberepresentedinamachineword. Forour
purposes,itisconvenienttokeepthelistwithleast-significantbigitfirst.
Wecandefinetherepresentationinductivelyby
(cid:2)
() n = 0
(cid:13) n (cid:14)=
|                | (cid:13) (cid:14)) = + | , ≤ <                |            |
| -------------- | ---------------------- | -------------------- | ---------- |
| (r .           | q n qN                 | r 0 r N              |            |
| =              | (cid:13) (cid:14)=(1   | (cid:13) (cid:14)=(2 |            |
| Soif N 16,then | 33 2)and               | 258                  | 0 1),since |
|                | 258 = 2 ×              | 160+ 0 × 161+        | 1 × 162    |
Noneoftheseimplementationsenforcesdataabstraction.Thereisnothing
to prevent a client program from looking at the representation and deter-
mining whether it is a list or a Scheme integer. On the other hand, some
languagesprovide directsupport for dataabstractions: they allow the pro-
grammertocreatenewinterfacesandcheckthatthenewdataismanipulated
onlythroughtheproceduresintheinterface. Ifthe representationofatype
ishidden,soitcannotbeexposedbyanyoperation(includingprinting),the
| typeissaidtobeopaque. | Otherwise,itissaidtobetransparent. |     |     |
| --------------------- | ---------------------------------- | --- | --- |
Schemedoesnotprovideastandardmechanismforcreatingnewopaque
types. Thus we settle for an intermediate level of abstraction: we define
interfacesandrelyonthewriteroftheclientprogramtobediscreetanduse
onlytheproceduresintheinterfaces.
Inchapter8,wediscusswaysinwhichalanguagecanenforcesuchproto-
cols.
(cid:3)
Exercise2.1 [ ] Implement the four required operations for bigits. Then use your
implementation to calculate the factorial of 10. How does the execution time vary
asthisargumentchanges? Howdoestheexecutiontimevaryasthebasechanges?
Explainwhy.
(cid:3)(cid:3)
Exercise2.2 [ ]Analyzeeachoftheseproposedrepresentationscritically. Towhat
extentdotheysucceedorfailinsatisfyingthespecificationofthedatatype?
(cid:3)(cid:3)
Exercise2.3 [ ]Definearepresentationofalltheintegers(negativeandnonnega-
tive)asdiff-trees,whereadiff-treeisalistdefinedbythegrammar
| Diff-tree::=(one) |     | |               |            |
| ----------------- | --- | --------------- | ---------- |
|                   |     | (diff Diff-tree | Diff-tree) |

2.2 RepresentationStrategiesforDataTypes 35
The list (one) represents 1. If t represents n and t represents n , then
1 1 2 2
(diff t t )isarepresentationofn −n .
1 2 1 2
Soboth(one)and(diff (one) (diff (one) (one)))arerepresentationsof
1;(diff (diff (one) (one)) (one))isarepresentationof−1.
1. Showthateverynumberhasinfinitelymanyrepresentationsinthissystem.
2. Turnthisrepresentationoftheintegersintoanimplementationbywritingzero,
is-zero?,successor,andpredecessor,asspecifiedonpage32,exceptthat
nowthe negativeintegersarealsorepresented. Yourproceduresshouldtakeas
inputanyofthemultiplelegalrepresentationsofanintegerinthisscheme. For
example,ifyoursuccessorprocedureisgivenanyoftheinfinitelymanylegal
representationsof1,itshouldproduceoneofthelegalrepresentationsof2. Itis
permissiblefor differentlegal representationsof 1 to yielddifferentlegal repre-
sentationsof2.
3. Write a procedure diff-tree-plus that does addition in this representation.
Yourprocedureshouldbeoptimizedforthediff-treerepresentation,andshould
doitsworkinaconstantamountoftime(independentofthesizeofitsinputs).In
particular,itshouldnotberecursive.
2.2 RepresentationStrategiesfor Data Types
Whendataabstractionisused,programshavethepropertyofrepresentation
independence: programs are independent of the particular representation
used to implement an abstract data type. It is then possible to change the
representation by redefining the small number of procedures belonging to
theinterface.Wefrequentlyrelyonthispropertyinlaterchapters.
In this section we introduce some strategies for representing data types.
Weillustratethesechoicesusingadatatypeofenvironments.Anenvironment
associatesavaluewitheachelementofafinitesetofvariables. Anenviron-
mentmaybeusedtoassociatevariableswiththeirvaluesinaprogramming
languageimplementation. Acompilermayalsouseanenvironmenttoasso-
ciateeachvariablenamewithinformationaboutthatvariable.
Variables may be represented in any way we please, so long as we can
check two variables for equality. We choose to represent variables using
Scheme symbols, but in a language without a symbol data type, variables
could be representedby strings, by referencesinto a hash table, or evenby
numbers(seesection3.6).

36 2 DataAbstraction
2.2.1 TheEnvironmentInterface
Anenvironmentisafunctionwhose domainisafinitesetofvariables,and
whoserangeisthesetofallSchemevalues. Sinceweadopttheusualmath-
ematicalconventionthatafinitefunctionisafinitesetoforderedpairs,then
weneedtorepresentallsetsoftheform { (var , val ) ,..., (var , val ) } where
|     |     |     | 1 1 | n n |
| --- | --- | --- | --- | --- |
thevar aredistinctvariablesandtheval areanySchemevalues. Wesome-
| i   |     |     | i   |     |
| --- | --- | --- | --- | --- |
times call the value of the variable var in an environment env its binding in
env.
Theinterfacetothisdatatypehasthreeprocedures,specifiedasfollows:
| (empty-env) |                    | =(cid:13)∅(cid:14)  |         |       |
| ----------- | ------------------ | ------------------- | ------- | ----- |
|             | (cid:13) (cid:14)  | =                   |         |       |
| (apply-env  | f var)             | f(var)              |         |       |
| (extend-env | (cid:13) (cid:14)) | =(cid:13) (cid:14), |         |       |
|             | var v f            | g                   | (cid:2) |       |
|             |                    |                     | v ifvar | = var |
|             |                    | whereg(var          | ) =     | 1     |
1
|     |     |     | f(var 1 ) otherwise |     |
| --- | --- | --- | ------------------- | --- |
Theprocedureempty-env,appliedtonoarguments,mustproducearepre-
sentationoftheemptyenvironment;apply-envappliesarepresentationof
anenvironment toavariableand(extend-env var val env)producesa
newenvironmentthatbehaveslike env,exceptthatitsvalueatvariablevar
isval.Forexample,theexpression
| > (define   | e           |       |     |     |
| ----------- | ----------- | ----- | --- | --- |
| (extend-env | ’d 6        |       |     |     |
| (extend-env | ’y          | 8     |     |     |
| (extend-env | ’x          | 7     |     |     |
|             | (extend-env | ’y 14 |     |     |
(empty-env))))))
|     |     | =   | = = |     |
| --- | --- | --- | --- | --- |
definesanenvironmentesuchthate(d) 6,e(x) 7,e(y) 8,andeisunde-
fined on any other variables. This is, of course, only one of many different
ways of building this environment. For instance, in the example above the
bindingofyto14isoverriddenbyitslaterbindingto8.
As in the previous example, we can divide the procedures of the inter-
face into constructors and observers. In this example, empty-env and
extend-envaretheconstructors,andapply-envistheonlyobserver.

2.2 RepresentationStrategiesforDataTypes 37
(cid:3)(cid:3)
Exercise2.4 [ ] Considerthedatatypeofstacksofvalues,withaninterfaceconsist-
ingoftheproceduresempty-stack,push,pop,top,andempty-stack?.Writea
specificationfortheseoperationsinthestyleoftheexampleabove.Whichoperations
areconstructorsandwhichareobservers?
2.2.2 DataStructureRepresentation
We can obtain a representation of environments by observing that every
environmentcanbebuiltbystartingwiththeemptyenvironmentandapply-
ingextend-envntimes,forsomen ≥ 0,e.g.,
(extend-env var val
n n
...
(extend-env var val
1 1
(empty-env))...)
Soeveryenvironmentcanbebuiltbyanexpressioninthefollowinggram-
mar:
Env-exp::=(empty-env)
::=(extend-env Identifier Scheme-value Env-exp)
We could representenvironments using the same grammar to describe a
set of lists. This would give the implementation shown in figure 2.1. The
procedureapply-envlooksatthedatastructureenvrepresentinganenvi-
ronment, determines what kind of environment it represents, and does the
rightthing. Ifitrepresentstheemptyenvironment,thenanerrorisreported.
Ifitrepresentsanenvironmentbuiltbyextend-env,thenitcheckstoseeif
thevariableitislookingforisthesameastheoneboundintheenvironment.
Ifitis,thenthesavedvalueisreturned.Otherwise,thevariableislookedup
inthesavedenvironment.
Thisisaverycommonpatternofcode. Wecallittheinterpreterrecipe:
TheInterpreterRecipe
1. Lookatapieceofdata.
2. Decidewhatkindofdataitrepresents.
3. Extract the components of the datum and do the right thing with
them.

| 38  |             |     |               |     |               |     | 2 DataAbstraction |
| --- | ----------- | --- | ------------- | --- | ------------- | --- | ----------------- |
| =   | (empty-env) |     | | (extend-env |     |               |     | Env)              |
| Env |             |     |               |     | Var SchemeVal |     |                   |
Var = Sym
→
| empty-env  | : ()        | Env         |               |           |        |     |     |
| ---------- | ----------- | ----------- | ------------- | --------- | ------ | --- | --- |
| (define    | empty-env   |             |               |           |        |     |     |
| (lambda    | ()          | (list       | ’empty-env))) |           |        |     |     |
|            | :           | ×           |               | ×         | →      |     |     |
| extend-env | Var         |             | SchemeVal     |           | Env    | Env |     |
| (define    | extend-env  |             |               |           |        |     |     |
| (lambda    | (var        | val         | env)          |           |        |     |     |
| (list      | ’extend-env |             | var           | val       | env))) |     |     |
| apply-env  | : Env       | ×           | Var →         | SchemeVal |        |     |     |
| (define    | apply-env   |             |               |           |        |     |     |
| (lambda    | (env        | search-var) |               |           |        |     |     |
(cond
|     | ((eqv?                   | (car       | env) ’empty-env)  |         |              |     |     |
| --- | ------------------------ | ---------- | ----------------- | ------- | ------------ | --- | --- |
|     | (report-no-binding-found |            |                   |         | search-var)) |     |     |
|     | ((eqv?                   | (car       | env) ’extend-env) |         |              |     |     |
|     | (let ((saved-var         |            |                   | (cadr   | env))        |     |     |
|     |                          | (saved-val |                   | (caddr  | env))        |     |     |
|     |                          | (saved-env |                   | (cadddr | env)))       |     |     |
|     | (if                      | (eqv?      | search-var        |         | saved-var)   |     |     |
saved-val
|     | (apply-env |     | saved-env |     | search-var)))) |     |     |
| --- | ---------- | --- | --------- | --- | -------------- | --- | --- |
(else
|             | (report-invalid-env     |                                             |     | env))))) |              |     |                   |
| ----------- | ----------------------- | ------------------------------------------- | --- | -------- | ------------ | --- | ----------------- |
| (define     | report-no-binding-found |                                             |     |          |              |     |                   |
| (lambda     | (search-var)            |                                             |     |          |              |     |                   |
| (eopl:error |                         | ’apply-env                                  |     | "No      | binding      | for | ~s" search-var))) |
| (define     | report-invalid-env      |                                             |     |          |              |     |                   |
| (lambda     | (env)                   |                                             |     |          |              |     |                   |
| (eopl:error |                         | ’apply-env                                  |     | "Bad     | environment: |     | ~s" env)))        |
|             | Figure2.1               | Adata-structurerepresentationofenvironments |     |          |              |     |                   |

2.2 RepresentationStrategiesforDataTypes 39
(cid:2)
Exercise2.5 [ ]Wecanuseanydatastructureforrepresentingenvironments,ifwe
can distinguish empty environments from non-empty ones, and in which one can
extract the pieces of a non-empty environment. Implement environments using a
representationinwhichtheemptyenvironmentisrepresentedastheemptylist,and
inwhichextend-envbuildsanenvironmentthatlookslike
saved−env
|     | saved−var | saved−val |     |
| --- | --------- | --------- | --- |
Thisiscalledana-listorassociation-listrepresentation.
(cid:2)
Exercise2.6 [ ] Invent at least three different representations of the environment
interfaceandimplementthem.
| (cid:2) | apply-env |     |     |
| ------- | --------- | --- | --- |
Exercise2.7 [ ] Rewrite in figure 2.1 to give a more informative error
message.
Exercise2.8 [ (cid:2) ] Add to the environment interface an observercalled empty-env?
andimplementitusingthea-listrepresentation.
(cid:2)
Exercise2.9 [ ]Addtotheenvironmentinterfaceanobservercalledhas-binding?
thattakesanenvironmentenvandavariablesandteststoseeifshasanassociated
valueinenv.Implementitusingthea-listrepresentation.
(cid:2)
Exercise2.10 [ ]Addtotheenvironmentinterfaceaconstructorextend-env*,and
implementitusingthea-listrepresentation.Thisconstructortakesalistofvariables,
alistofvaluesofthesamelength,andanenvironment,andisspecifiedby
| (extend-env* | (v(cid:2)ar ... | var ) (val ... | val ) (cid:2)f(cid:3))=(cid:2)g(cid:3), |
| ------------ | --------------- | -------------- | --------------------------------------- |
|              | 1               | k 1            | k                                       |
ifvar=var forsomeisuchthat1≤i≤k
| whereg(var)=   | val i  | i           |     |
| -------------- | ------ | ----------- | --- |
|                | f(var) | otherwise   |     |
| (cid:2)(cid:2) |        | extend-env* |     |
Exercise2.11 [ ] A naive implementation of from the preceding
exerciserequirestime proportional to k to run. It is possible to representenviron-
mentssothatextend-env*requiresonlyconstanttime: representtheemptyenvi-
ronmentbytheemptylist,andrepresentanon-emptyenvironmentbythedatastruc-
ture
saved−env
|     | saved−vars | saved−vals |     |
| --- | ---------- | ---------- | --- |
Suchanenvironmentmightlooklike

40 2 DataAbstraction
backbone
rest of environment
(a b c) (11 12 13) (x z) (66 77) (x y) (88 99)
Thisiscalledtheribcagerepresentation. Theenvironmentisrepresentedasalistof
pairscalledribs;eachleftribisalistofvariablesandeachrightribisthecorrespond-
inglistofvalues.
Implementthe environment interface, including extend-env*, in this representa-
tion.
2.2.3 ProceduralRepresentation
The environment interface has an important property: it has exactly one
observer, apply-env. This allows us to represent an environment as a
Schemeprocedurethattakesavariableandreturnsitsassociatedvalue.
Todothis,wedefineempty-envandextend-envtoreturnprocedures
that,whenapplied,dothesamethingthatapply-envdidinthepreceding
section. Thisgivesusthefollowingimplementation.
Env = Var → SchemeVal
empty-env : () → Env
(define empty-env
(lambda ()
(lambda (search-var)
(report-no-binding-found search-var))))
extend-env : Var × SchemeVal × Env → Env
(define extend-env
(lambda (saved-var saved-val saved-env)
(lambda (search-var)
(if (eqv? search-var saved-var)
saved-val
(apply-env saved-env search-var)))))
apply-env : Env × Var → SchemeVal
(define apply-env
(lambda (env search-var)
(env search-var)))

2.2 RepresentationStrategiesforDataTypes 41
Iftheemptyenvironment,createdbyinvokingempty-env,ispassedany
variablewhatsoever, it indicateswith anerror message thatthe given vari-
able is not in its domain. The procedure extend-env returns a new pro-
cedure that represents the extended environment. This procedure, when
passedavariablesearch-var,checkstoseeifthevariableitislookingfor
isthesameastheoneboundintheenvironment. Ifitis,thenthesavedvalue
isreturned.Otherwise,thevariableislookedupinthesavedenvironment.
Wecallthisaproceduralrepresentation,inwhichthedataisrepresentedby
itsactionunderapply-env.
The caseof adatatypewith asingle observeris lessrarethanone might
think. Forexample,ifthedatabeingrepresentedisasetoffunctions,thenit
canberepresentedbyitsactionunderapplication.Inthiscase,wecanextract
theinterfaceandtheproceduralrepresentationbythefollowingrecipe:
1. Identifythelambdaexpressionsintheclientcodewhoseevaluationyields
valuesofthe type. Createaconstructor procedureforeachsuchlambda
expression. Theparametersofthe constructor procedurewillbethe free
variablesofthelambdaexpression.Replaceeachsuchlambdaexpression
intheclientcodebyaninvocationofthecorrespondingconstructor.
2. Define an apply- procedure like apply-env above. Identify all the
places in the client code, including the bodies of the constructor proce-
dures,whereavalueofthetypeisapplied.Replaceeachsuchapplication
byaninvocationoftheapply-procedure.
If these steps are carried out, the interface will consist of all the con-
structor proceduresand the apply- procedure, and the client code will be
representation-independent: it will not rely on the representation, and we
willbefreetosubstituteanotherimplementationoftheinterface,suchasthe
onewedescribeinsection2.2.2.
If the implementation language does not allow higher-order procedures,
thenonecanperformtheadditionalstepofimplementingtheresultinginter-
faceusingadatastructurerepresentationandtheinterpreterrecipe,asinthe
precedingsection. This process is called defunctionalization. The derivation
ofthe datastructurerepresentationofenvironmentsisasimple exampleof
defunctionalization. The relationbetweenproceduralanddefunctionalized
representationswillbearecurringthemeinthisbook.

42 2 DataAbstraction
(cid:3)
Exercise2.12 [ ] Implement the stack data type of exercise 2.4 using a procedural
representation.
(cid:3)(cid:3) Extendtheproceduralrepresentationtoimplementempty-env?
| Exercise2.13 [ | ]   |     |
| -------------- | --- | --- |
by representing the environment by a list of two procedures: one that returns the
valueassociatedwithavariable,asbefore,andonethatreturnswhetherornotthe
environmentisempty.
Exercise2.14 [ (cid:3)(cid:3) ] Extendthe representationofthe precedingexerciseto includea
thirdprocedurethatimplementshas-binding?(seeexercise2.9).
| 2.3 InterfacesforRecursive |     | DataTypes |
| -------------------------- | --- | --------- |
Wespentmuchofchapter1manipulatingrecursivedatatypes. Forexample,
wedefinedlambda-calculusexpressionsindefinition1.1.8bythegrammar
Lc-exp::=Identifier
::=(lambda
(Identifier) Lc-exp)
|     | ::=(Lc-exp | Lc-exp) |
| --- | ---------- | ------- |
andwewroteprocedureslikeoccurs-free?.Aswementionedatthetime,
thedefinitionofoccurs-free?insection1.2.4isnotasreadableasitmight
be.Itishardtotell,forexample,that(car (cadr exp))referstothedec-
laration of a variable in a lambda expression, or that (caddr exp) refers
toitsbody.
We can improve this situation by introducing an interface for lambda-
calculus expressions. Our interface will have constructors and two kinds
| ofobservers: | predicatesandextractors. |     |
| ------------ | ------------------------ | --- |
Theconstructorsare:
→
| var-exp    | : Var Lc-exp    |        |
| ---------- | --------------- | ------ |
|            | ×               | →      |
| lambda-exp | : Var Lc-exp    | Lc-exp |
|            | ×               | →      |
| app-exp    | : Lc-exp Lc-exp | Lc-exp |
Thepredicatesare:
→
| var-exp? | : Lc-exp Bool |     |
| -------- | ------------- | --- |
→
| lambda-exp? | : Lc-exp Bool   |     |
| ----------- | --------------- | --- |
| app-exp?    | : Lc-exp → Bool |     |
Finally,theextractorsare

2.3 InterfacesforRecursiveDataTypes 43
→
var-exp->var : Lc-exp Var
→
lambda-exp->bound-var : Lc-exp Var
→
lambda-exp->body : Lc-exp Lc-exp
→
app-exp->rator : Lc-exp Lc-exp
→
app-exp->rand : Lc-exp Lc-exp
Each of these extracts the corresponding portion of the lambda-calculus
expression. We can now write a version of occurs-free? that depends
onlyontheinterface.
occurs-free? : Sym × LcExp → Bool
(define occurs-free?
(lambda (search-var exp)
(cond
((var-exp? exp) (eqv? search-var (var-exp->var exp)))
((lambda-exp? exp)
(and
(not (eqv? search-var (lambda-exp->bound-var exp)))
(occurs-free? search-var (lambda-exp->body exp))))
(else
(or
(occurs-free? search-var (app-exp->rator exp))
(occurs-free? search-var (app-exp->rand exp)))))))
Thisworksonanyrepresentationoflambda-calculusexpressions,solong
astheyarebuiltusingtheseconstructors.
Wecanwritedownageneralrecipefordesigninganinterfaceforarecur-
sivedatatype:
Designinganinterface forarecursivedatatype
1. Includeoneconstructorforeachkindofdatainthedatatype.
2. Includeonepredicate foreachkindofdatainthedatatype.
3. Include one extractor for each piece of data passed to a constructor
ofthedatatype.
(cid:3)
Exercise2.15 [ ] Implementthelambda-calculusexpressioninterfacefortherepre-
sentationspecifiedbythegrammarabove.
(cid:3)
Exercise2.16 [ ] Modifytheimplementationtousearepresentationinwhichthere
arenoparenthesesaroundtheboundvariableinalambdaexpression.

44 2 DataAbstraction
(cid:3)
Exercise2.17 [ ] Inventatleasttwootherrepresentationsofthedatatypeoflambda-
calculusexpressionsandimplementthem.
Exercise2.18 [ (cid:3) ]Weusuallyrepresentasequenceofvaluesasalist.Inthisrepresen-
tation, itiseasytomovefromone elementinasequenceto the next, butitishard
to move from one element to the preceding one without the help of context argu-
ments. Implement non-empty bidirectional sequences of integers, as suggestedby
thegrammar
|     | NodeInSequence::=(Int |     |     | Listof(Int) | Listof(Int)) |
| --- | --------------------- | --- | --- | ----------- | ------------ |
Thefirstlistofnumbersisthe elementsofthe sequenceprecedingthecurrentone,
inreverseorder,andthesecondlististheelementsofthesequenceafterthecurrent
one.Forexample,(6 (5 4 3 2 1) (7 8 9))representsthelist(1 2 3 4 5 6
7 8 9),withthefocusontheelement6.
Inthisrepresentation,implementthe procedurenumber->sequence,whichtakes
anumber and producesa sequenceconsisting of exactlythat number. Alsoimple-
ment current-element, move-to-left, move-to-right, insert-to-left,
insert-to-right,at-left-end?,andat-right-end?.
Forexample:
| > (number->sequence |     |     | 7)  |     |     |
| ------------------- | --- | --- | --- | --- | --- |
(7 () ())
| > (current-element |     |     | ’(6 | (5 4 3 2 1) | (7 8 9))) |
| ------------------ | --- | --- | --- | ----------- | --------- |
6
| > (move-to-left    |      | ’(6    | (5     | 4 3 2 1) (7  | 8 9)))       |
| ------------------ | ---- | ------ | ------ | ------------ | ------------ |
| (5 (4 3            | 2 1) | (6 7   | 8 9))  |              |              |
| > (move-to-right   |      |        | ’(6 (5 | 4 3 2 1) (7  | 8 9)))       |
| (7 (6 5            | 4 3  | 2 1)   | (8 9)) |              |              |
| > (insert-to-left  |      |        | 13 ’(6 | (5 4 3 2     | 1) (7 8 9))) |
| (6 (13             | 5 4  | 3 2 1) | (7 8   | 9))          |              |
| > (insert-to-right |      |        | 13     | ’(6 (5 4 3 2 | 1) (7 8 9))) |
| (6 (5 4            | 3 2  | 1) (13 | 7 8    | 9))          |              |
Theproceduremove-to-rightshouldfailifitsargumentisattherightendofthe
sequence,andtheproceduremove-to-leftshouldfailifitsargumentisattheleft
endofthesequence.
(cid:3)
Exercise2.19 [ ] A binary tree with empty leaves and with interior nodes labeled
withintegerscouldberepresentedusingthegrammar
|     |     | Bintree::=() |     | | (Int Bintree | Bintree) |
| --- | --- | ------------ | --- | -------------- | -------- |
Inthisrepresentation,implementtheprocedurenumber->bintree,whichtakesa
numberandproducesabinarytreeconsistingofasinglenodecontainingthatnum-
ber.Alsoimplementcurrent-element,move-to-left-son,move-to-right-
son,at-leaf?,insert-to-left,andinsert-to-right.Forexample,

| 2.4 | AToolforDefiningRecursiveDataTypes |     |     | 45  |
| --- | ---------------------------------- | --- | --- | --- |
| >   | (number->bintree                   | 13) |     |     |
(13 () ())
| >   | (define t1 (insert-to-right |                  | 14    |     |
| --- | --------------------------- | ---------------- | ----- | --- |
|     | (insert-to-left             |                  | 12    |     |
|     |                             | (number->bintree | 13))) |     |
> t1
(13
(12 () ())
(14 () ()))
| >   | (move-to-left | t1) |     |     |
| --- | ------------- | --- | --- | --- |
(12 () ())
| >   | (current-element | (move-to-left | t1)) |     |
| --- | ---------------- | ------------- | ---- | --- |
12
| >   | (at-leaf? (move-to-right |     | (move-to-left | t1))) |
| --- | ------------------------ | --- | ------------- | ----- |
#t
| >   | (insert-to-left | 15 t1) |     |     |
| --- | --------------- | ------ | --- | --- |
(13
(15
(12 () ())
())
(14 () ()))
Exercise2.20 [ (cid:3)(cid:3)(cid:3) ]Intherepresentationofbinarytreesinexercise2.19itiseasyto
movefromaparentnodetooneofitssons,butitisimpossibletomovefromasonto
itsparentwithoutthehelpofcontextarguments.Extendtherepresentationoflistsin
exercise2.18torepresentnodesinabinarytree. Asahint,considerrepresentingthe
portionofthetreeabovethecurrentnodebyareversedlist,asinexercise2.18.
Inthisrepresentation,implementtheproceduresfromexercise2.19.Alsoimplement
move-up,at-root?,andat-leaf?.
| 2.4 ATool | for DefiningRecursive |     | Data Types |     |
| --------- | --------------------- | --- | ---------- | --- |
Forcomplicateddatatypes,applyingtherecipeforconstructinganinterface
can quickly become tedious. In this section, we introduce a tool for auto-
matically constructing and implementing such interfaces in Scheme. The
interfaces constructed by this tool will be similar, but not identical, to the
interfaceconstructedintheprecedingsection.

46 2 DataAbstraction
Consideragainthedatatypeoflambda-calculusexpressions,asdiscussed
intheprecedingsection. Wecanimplementaninterfaceforlambda-calculus
expressionsbywriting
| (define-datatype |     | lc-exp | lc-exp? |     |     |
| ---------------- | --- | ------ | ------- | --- | --- |
(var-exp
|     | (var identifier?)) |     |     |     |     |
| --- | ------------------ | --- | --- | --- | --- |
(lambda-exp
|     | (bound-var      | identifier?) |     |     |     |
| --- | --------------- | ------------ | --- | --- | --- |
|     | (body lc-exp?)) |              |     |     |     |
(app-exp
|     | (rator lc-exp?)  |     |     |     |     |
| --- | ---------------- | --- | --- | --- | --- |
|     | (rand lc-exp?))) |     |     |     |     |
Herethenamesvar-exp,var,bound-var,app-exp,rator,andrand
abbreviate variable expression, variable, bound variable, application expression,
operator,andoperand,respectively.
var-exp,lambda-exp,and
Thisexpressiondeclaresthreeconstructors,
app-exp, and a single predicate lc-exp?. The three constructors check
their arguments with the predicates identifier? and lc-exp? to make
sure that the arguments are valid, so if an lc-exp is constructed using only
these constructors, we can be certain that it and all its subexpressions are
legallc-exps.Thisallowsustoignoremanycheckswhileprocessinglambda
expressions.
Inplaceofthevariouspredicatesandextractors,weusetheformcasesto
determinethevarianttowhichanobjectofadatatypebelongs,andtoextract
its components. To illustrate this form, we can rewrite occurs-free?
(page43)usingthedatatypelc-exp:
× →
| occurs-free? | : Sym         | LcExp      |       | Bool             |     |
| ------------ | ------------- | ---------- | ----- | ---------------- | --- |
| (define      | occurs-free?  |            |       |                  |     |
| (lambda      | (search-var   |            | exp)  |                  |     |
|              | (cases lc-exp | exp        |       |                  |     |
|              | (var-exp      | (var)      | (eqv? | var search-var)) |     |
|              | (lambda-exp   | (bound-var |       | body)            |     |
(and
|     | (not          | (eqv?  | search-var |     | bound-var)) |
| --- | ------------- | ------ | ---------- | --- | ----------- |
|     | (occurs-free? |        | search-var |     | body)))     |
|     | (app-exp      | (rator | rand)      |     |             |
(or
|     | (occurs-free? |     | search-var |     | rator)     |
| --- | ------------- | --- | ---------- | --- | ---------- |
|     | (occurs-free? |     | search-var |     | rand)))))) |

2.4 AToolforDefiningRecursiveDataTypes 47
To see how this works, assume that exp is a lambda-calculusexpression
thatwasbuiltbyapp-exp. Forthisvalueofexp,theapp-exp casewould
be selected, rator and rand would be bound to the two subexpressions,
andtheexpression
(or
(occurs-free? search-var rator)
(occurs-free? search-var rand))
wouldbeevaluated,justasifwehadwritten
(if (app-exp? exp)
(let ((rator (app-exp->rator exp))
(rand (app-exp->rand exp)))
(or
(occurs-free? search-var rator)
(occurs-free? search-var rand)))
...)
The recursive calls to occurs-free? work similarly to finish the calcula-
tion.
Ingeneral,adefine-datatypedeclarationhastheform
(define-datatype type-name type-predicate-name
{(variant-name {(field-name predicate)}∗)}+)
Thiscreatesadatatype,namedtype-name,withsomevariants.Eachvariant
hasavariant-nameandzeroormorefields,eachwithitsownfield-nameand
associated predicate. No two types may have the same name and no two
variants,eventhose belongingtodifferenttypes,mayhavethesamename.
Also,typenamescannotbeusedasvariantnames.Eachfieldpredicatemust
beaSchemepredicate.
For each variant, a new constructor procedure is created that is used to
create data values belonging to that variant. These procedures are named
after their variants. If there are n fields in a variant, its constructor takes
narguments, testseachof themwithitsassociatedpredicate,andreturnsa
newvalueofthegivenvariantwiththei-thfieldcontainingthei-thargument
value.
Thetype-predicate-nameisboundtoapredicate. Thispredicatedetermines
ifitsargumentisavaluebelongingtothenamedtype.

48 2 DataAbstraction
Arecordcanbedefinedasadatatypewithasinglevariant.Todistinguish
datatypeswithonlyonevariant,weuseanamingconvention. Whenthere
is a single variant, we name the constructor a-type-name or an-type-name;
otherwise,theconstructorshavenameslikevariant-name-type-name.
Data types built by define-datatypemay be mutually recursive. For
example,considerthegrammarfors-listsfromsection1.1:
S-list ::=({S-exp}∗)
|     | S-exp::=Symbol | |   |
| --- | -------------- | --- |
S-list
Thedatainans-listcouldberepresentedbythedatatypes-listdefined
by
| (define-datatype | s-list s-list? |     |
| ---------------- | -------------- | --- |
(empty-s-list)
(non-empty-s-list
(first s-exp?)
(rest s-list?)))
| (define-datatype | s-exp s-exp? |     |
| ---------------- | ------------ | --- |
(symbol-s-exp
(sym symbol?))
(s-list-s-exp
(slst s-list?)))
The data type s-list gives its own representation of lists by using
(empty-s-list)andnon-empty-s-listinplaceof()andcons;ifwe
wantedtospecifythatSchemelistsbeusedinstead,wecouldhavewritten
| (define-datatype | s-list s-list? |     |
| ---------------- | -------------- | --- |
(an-s-list
| (sexps (list-of | s-exp?)))) |     |
| --------------- | ---------- | --- |
(define list-of
(lambda (pred)
(lambda (val)
(or (null? val)
| (and (pair? | val)       |            |
| ----------- | ---------- | ---------- |
| (pred (car  | val))      |            |
| ((list-of   | pred) (cdr | val))))))) |
Here(list-of pred)buildsapredicatethatteststoseeifitsargumentisa
list,andthateachofitselementssatisfiespred.

2.4 AToolforDefiningRecursiveDataTypes 49
Thegeneralsyntaxofcasesis
| (cases        | type-name expression |               |     |
| ------------- | -------------------- | ------------- | --- |
| {             | {                    | }∗            | }∗  |
| (variant-name | ( field-name         | ) consequent) |     |
| (else         | default))            |               |     |
Theformspecifiesthetype,theexpressionyieldingthevaluetobeexamined,
andasequenceofclauses. Eachclauseislabeledwiththenameofavariant
of the given type and the names of its fields. The else clause is optional.
First, expression is evaluated, resulting in some value v of type-name. If v is
avariantofvariant-name, thenthecorrespondingclauseisselected. Eachof
the field-names is bound to the value of the corresponding field of v. Then
the consequent is evaluatedwithin the scope of these bindings and its value
returned. If v isnot one of the variants,and anelseclause hasbeenspec-
elseclause,
| ified, default | is evaluatedand | itsvalue returned. | Ifthere isno |
| -------------- | --------------- | ------------------ | ------------ |
thentheremustbeaclauseforeveryvariantofthatdatatype.
Theformcasesbindsitsvariablespositionally: thei-thvariableisbound
| tothevalueinthei-thfield. |                | Sowecouldjustaswellhavewritten |     |
| ------------------------- | -------------- | ------------------------------ | --- |
|                           | (app-exp (exp1 | exp2)                          |     |
(or
|     | (occurs-free? | search-var | exp1)   |
| --- | ------------- | ---------- | ------- |
|     | (occurs-free? | search-var | exp2))) |
insteadof
|     | (app-exp (rator | rand) |     |
| --- | --------------- | ----- | --- |
(or
|     | (occurs-free? | search-var | rator)  |
| --- | ------------- | ---------- | ------- |
|     | (occurs-free? | search-var | rand))) |
The forms define-datatype and cases provide a convenient way of
defining an inductive data type, but it is not the only way. Depending on
the application, it may be valuable to use a special-purpose representation
that is more compact or efficient, taking advantage of special properties of
thedata. Theseadvantagesaregainedattheexpenseofhavingtowritethe
proceduresintheinterfacebyhand.
The formdefine-datatypeisanexampleofa domain-specificlanguage.
Adomain-specificlanguageisasmalllanguagefordescribingasingle task
amongasmall, well-definedsetoftasks. Inthiscase,thetaskwasdefining
arecursivedatatype. Suchalanguagemaylieinsideageneral-purposelan-
guage,asdefine-datatypedoes,oritmaybeastandalonelanguagewith

50 2 DataAbstraction
itsownsetoftools. Ingeneral,oneconstructssuchalanguagebyidentifying
thepossiblevariationsinthesetoftasks,andthendesigningalanguagethat
describesthosevariations. Thisisoftenaveryusefulstrategy.
(cid:3)
Exercise2.21 [ ] Implementthedatatypeofenvironments,asinsection2.2.2,using
define-datatype.Thenincludehas-binding?ofexercise2.9.
(cid:3)
Exercise2.22 [ ] Usingdefine-datatype,implementthestackdatatypeofexer-
cise2.4.
(cid:3)
Exercise2.23 [ ] The definition of lc-exp ignoresthe condition in definition 1.1.8
that says “Identifier is any symbol other than lambda.” Modify the definition of
identifier? to capture this condition. As a hint, remember that any predicate
canbeusedindefine-datatype,evenonesyoudefine.
(cid:3)
Exercise2.24 [ ] Hereisadefinitionofbinarytreesusingdefine-datatype.
(define-datatype bintree bintree?
(leaf-node
(num integer?))
(interior-node
(key symbol?)
(left bintree?)
(right bintree?)))
Implementabintree-to-listprocedureforbinarytrees,sothat(bintree-to-
list (interior-node ’a (leaf-node 3) (leaf-node 4)))returnsthelist
(interior-node
a
(leaf-node 3)
(leaf-node 4))
(cid:3)(cid:3)
Exercise2.25 [ ] Usecasestowritemax-interior,whichtakesabinarytreeof
integers(asintheprecedingexercise)withatleastoneinteriornodeandreturnsthe
symbolassociatedwithaninteriornodewithamaximalleafsum.
> (define tree-1
(interior-node ’foo (leaf-node 2) (leaf-node 3)))
> (define tree-2
(interior-node ’bar (leaf-node -1) tree-1))
> (define tree-3
(interior-node ’baz tree-2 (leaf-node 1)))
> (max-interior tree-2)
foo
> (max-interior tree-3)
baz
Thelastinvocationofmax-interiormightalsohavereturnedfoo,sinceboththe
fooandbaznodeshavealeafsumof5.

2.5 AbstractSyntaxandItsRepresentation 51
(cid:3)(cid:3)
Exercise2.26 [ ] Here is another version of exercise1.33. Consider a set of trees
givenbythefollowinggrammar:
Red-blue-tree ::=Red-blue-subtree
Red-blue-subtree::=(red-node Red-blue-subtree Red-blue-subtree)
::=(blue-node {Red-blue-subtree}∗)
::=(leaf-node Int)
Writeanequivalentdefinitionusingdefine-datatype,andusetheresultinginter-
facetowriteaprocedurethattakesatreeandbuildsatreeofthesameshape,except
thateachleafnodeisreplacedbyaleafnodethatcontainsthenumberofrednodes
onthepathbetweenitandtheroot.
2.5 AbstractSyntax and ItsRepresentation
Agrammarusuallyspecifiesaparticularrepresentationofaninductivedata
type: onethatusesthe stringsor valuesgeneratedbythe grammar. Sucha
representationiscalledconcretesyntax,orexternalrepresentation.
Consider, for example, the set of lambda-calculusexpressions defined in
definition 1.1.8. This gives a concrete syntax for lambda-calculus expres-
sions. Wemighthaveusedsomeotherconcretesyntaxforlambda-calculus
expressions. Forexample,wecouldhavewritten
Lc-exp::=Identifier
::=proc Identifier => Lc-exp
::=Lc-exp(Lc-exp)
todefinelambda-calculusexpressionsasadifferentsetofstrings.
Inordertoprocesssuchdata,weneedtoconvertittoaninternalrepresen-
tation. Thedefine-datatypeformprovidesaconvenientwayofdefining
such an internal representation. We call this abstract syntax. In the abstract
syntax,terminalssuchasparenthesesneednotbestored,becausetheycon-
veynoinformation. Ontheotherhand,wewanttomakesurethatthedata
structureallowsustodeterminewhatkindoflambda-calculusexpressionit
represents,andtoextractitscomponents. Thedatatypelc-exponpage46
allowsustodobothofthesethingseasily.
Itis convenient tovisualize the internal representationasan abstract syn-
tax tree. Figure 2.2 shows the abstract syntax tree of the lambda-calculus
expression(lambda (x) (f (f x))),usingthedatatypelc-exp. Each
internal node of the tree is labeled with the associated production name.
Edges are labeled with the name of the corresponding nonterminal occur-
rence.Leavescorrespondtoterminalstrings.

| 52                                     |     |     |        | 2 DataAbstraction |
| -------------------------------------- | --- | --- | ------ | ----------------- |
| Figure2.2 Abstractsyntaxtreefor(lambda |     |     | (x) (f | (f x)))           |
To create an abstract syntax for a given concrete syntax, we must name
eachproductionoftheconcretesyntaxandeachoccurrenceofanonterminal
in each production. It is straightforward to generate define-datatype
declarationsfor the abstractsyntax. We create one define-datatypefor
eachnonterminal,withonevariantforeachproduction.
We can summarize the choices we have made in figure 2.2 using the fol-
lowingconcisenotation:
Lc-exp::=Identifier
|     | var-exp    | (var)                |       |     |
| --- | ---------- | -------------------- | ----- | --- |
|     | ::=(lambda | (Identifier) Lc-exp) |       |     |
|     | lambda-exp | (bound-var           | body) |     |
::=(Lc-exp Lc-exp)
|     | app-exp | (rator rand) |     |     |
| --- | ------- | ------------ | --- | --- |
Such notation, which specifies both concrete and abstractsyntax, is used
throughoutthisbook.
Havingmadethedistinctionbetweenconcretesyntax,whichisprimarily
useful for humans, and abstract syntax, which is primarily useful for com-
puters,wenowconsiderhowtoconvertfromonesyntaxtotheother.

2.5 AbstractSyntaxandItsRepresentation 53
Iftheconcretesyntaxisasetofstringsofcharacters,itmaybeacomplex
undertaking to derive the corresponding abstract syntax tree. This task is
calledparsingand isperformedbyaparser. Becausewriting aparserisdif-
ficult in general, it is best performed by a tool called a parser generator. A
parsergeneratortakesasinputagrammarandproducesaparser. Sincethe
grammars are processed by a tool, they must be written in some machine-
readablelanguage: adomain-specificlanguageforwritinggrammars.There
aremanyparsergeneratorsavailable.
If the concrete syntax is given as a set of lists, the parsing process is
considerably simplified. For example, the grammar for lambda-calculus
expressions at the beginning of this section specified a set of lists, as did
the grammar for define-datatypeon page 47. In this case, the Scheme
read routine automatically parses strings into lists and symbols. It is
then easier to parse these list structures into abstract syntax trees as in
parse-expression.
parse-expression : SchemeVal → LcExp
(define parse-expression
(lambda (datum)
(cond
((symbol? datum) (var-exp datum))
((pair? datum)
(if (eqv? (car datum) ’lambda)
(lambda-exp
(car (cadr datum))
(parse-expression (caddr datum)))
(app-exp
(parse-expression (car datum))
(parse-expression (cadr datum)))))
(else (report-invalid-concrete-syntax datum)))))
It is usually straightforward to convert an abstract syntax tree back to
a list-and-symbol representation. If we do this, the Scheme print routines
will then display it in a list-based concrete syntax. This is performed by
unparse-lc-exp:
unparse-lc-exp : LcExp → SchemeVal
(define unparse-lc-exp
(lambda (exp)
(cases lc-exp exp
(var-exp (var) var)
(lambda-exp (bound-var body)
(list ’lambda (list bound-var)
(unparse-lc-exp body)))
(app-exp (rator rand)
(list

54 2 DataAbstraction
(unparse-lc-exp rator) (unparse-lc-exp rand))))))
(cid:3)
Exercise2.27 [ ] Drawtheabstractsyntaxtreeforthelambdacalculusexpressions
((lambda (a) (a b)) c)
(lambda (x)
(lambda (y)
((lambda (x)
(x y))
x)))
(cid:3)
Exercise2.28 [ ] Writeanunparserthatconvertstheabstractsyntaxofanlc-expinto
astringthatmatchesthesecondgrammarinthissection(page52).
(cid:3)
Exercise2.29 [ ] WhereaKleenestarorplus(page7)isusedinconcretesyntax, it
ismostconvenienttousealistofassociatedsubtreeswhenconstructinganabstract
syntaxtree.Forexample,ifthegrammarforlambda-calculusexpressionshadbeen
Lc-exp::=Identifier
var-exp (var)
::=(lambda ({Identifier}∗) Lc-exp)
lambda-exp (bound-vars body)
::=(Lc-exp {Lc-exp}∗)
app-exp (rator rands)
thenthe predicateforthebound-varsfieldcouldbe(list-of identifier?),
and the predicate for the rands field could be (list-of lc-exp?). Write a
define-datatypeandaparserforthisgrammarthatworksinthisway.
(cid:3)(cid:3)
Exercise2.30 [ ] Theprocedureparse-expressionasdefinedaboveisfragile:
itdoesnotdetectseveralpossiblesyntacticerrors,suchas(a b c),andabortswith
inappropriateerrormessagesforotherexpressions,suchas(lambda). Modifyitso
thatitisrobust,acceptinganys-expandissuinganappropriateerrormessageifthe
s-expdoesnotrepresentalambda-calculusexpression.

2.5 AbstractSyntaxandItsRepresentation 55
(cid:3)(cid:3)
Exercise2.31 [ ]Sometimesitisusefultospecifyaconcretesyntaxasasequence
ofsymbolsandintegers,surroundedbyparentheses. Forexample,onemightdefine
thesetofprefixlistsby
::=(Prefix-exp)
Prefix-list
Prefix-exp::=Int
::=-
Prefix-exp Prefix-exp
sothat(- - 3 2 - 4 - 12 7)isalegalprefixlist.ThisissometimescalledPolish
prefixnotation,afteritsinventor,JanŁukasiewicz. Writeaparsertoconvertaprefix-
listtotheabstractsyntax
| (define-datatype | prefix-exp | prefix-exp? |
| ---------------- | ---------- | ----------- |
(const-exp
(num integer?))
(diff-exp
| (operand1 | prefix-exp?)   |     |
| --------- | -------------- | --- |
| (operand2 | prefix-exp?))) |     |
sothattheexampleaboveproducesthesameabstractsyntaxtreeasthesequenceof
constructors
(diff-exp
(diff-exp
| (const-exp | 3)  |     |
| ---------- | --- | --- |
| (const-exp | 2)) |     |
(diff-exp
| (const-exp | 4)  |     |
| ---------- | --- | --- |
(diff-exp
| (const-exp | 12)   |     |
| ---------- | ----- | --- |
| (const-exp | 7)))) |     |
Asahint,considerwritingaprocedurethattakesalistandproducesaprefix-exp
andthelistofleftoverlistelements.

3
Expressions
Inthischapter,westudythebindingandscopingofvariables.Wedothisby
presentingasequence of smalllanguagesthatillustrate these concepts. We
write specifications for these languages, and implement them using inter-
preters, following the interpreter recipe from chapter 1. Our specifications
andinterpreterstakeacontextargument,calledtheenvironment,whichkeeps
trackofthemeaningofeachvariableintheexpressionbeingevaluated.
3.1 Specification and ImplementationStrategy
Ourspecificationwillconsistofassertionsoftheform
(value-of exp ρ ) = val
ρ
meaningthatthevalueofexpressionexpinenvironment shouldbeval.We
writedownrulesofinferenceandequations,likethoseinchapter1,thatwill
enableustoderivesuchassertions. Weusetherulesandequationsbyhand
tofindtheintendedvalueofsomeexpressions.
But our goal is to write a program that implements our language. The
overall picture is shown in figure 3.1(a). We start with the text of the pro-
gramwritteninthelanguageweareimplementing. Thisiscalledthesource
languageor the definedlanguage. Programtext(aprogramin the source lan-
guage) is passed through a front end that converts it to an abstract syntax
tree. The syntax tree is then passed to the interpreter, which is a program
thatlooks atadatastructureandperformssome actionsthatdependonits
structure. Of course the interpreter is itself written in some language. We
callthatlanguagethe implementationlanguageor thedefininglanguage. Most
ofourimplementationswillfollowthispattern.

58 3 Expressions
Another common organizationis shown infigure 3.1(b). Therethe inter-
preterisreplacedbyacompiler,whichtranslatestheabstractsyntaxtreeinto
aprograminsomeotherlanguage(thetargetlanguage),andthatprogramis
executed. Thattargetlanguagemaybeexecutedbyaninterpreter,asinfig-
ure 3.1(b), or it may be translated into some even lower-level language for
execution.
Mostoften,thetargetlanguageisamachinelanguage,whichisinterpreted
byahardwaremachine. Yetanotherpossibilityisthatthetargetmachineis
a special-purpose language that is simpler than the original and for which
itisrelativelysimple towrite aninterpreter. Thisallows theprogramtobe
compiledonceandthenexecutedonmanydifferenthardwareplatforms.For
historical reasons, such a target language is often called a byte code, and its
interpreteriscalledavirtualmachine.
A compiler is typically divided into two parts: an analyzer that attempts
to deduce useful information about the program, and a translator that does
thetranslation, possiblyusinginformationfromtheanalyzer. Eachofthese
phases may be specified either by rules of inference or a special-purpose
specificationlanguage, andthenimplemented. Westudysome simple ana-
lyzersandtranslatorsinchapters6and7.
No matter what implementation strategy we use, we need a front end
that converts programs into abstract syntax trees. Because programs are
juststringsofcharacters,ourfrontendneedstogroupthesecharactersinto
meaningfulunits. Thisgroupingisusuallydividedintotwostages: scanning
andparsing.
Scanningistheprocessofdividingthesequenceofcharactersintowords,
numbers, punctuation, comments, and the like. These units are called lexi-
calitems, lexemes, or most often tokens. We refer to the way in which a pro-
gramshould be divided up into tokens as the lexical specificationof the lan-
guage. Thescannertakesasequenceofcharactersandproducesasequence
oftokens.
Parsing is the process of organizing the sequence of tokens into hierar-
chicalsyntacticstructuressuchasexpressions,statements, andblocks. This
is like organizing (diagramming) a sentence into clauses. We refer to this
asthe syntacticor grammaticalstructure of the language. The parser takesa
sequenceoftokensfromthescannerandproducesanabstractsyntaxtree.
Thestandardapproachtobuildingafrontendistouseaparsergenerator.
Aparsergeneratorisaprogramthattakesasinputalexicalspecificationand
agrammar,andproducesasoutputascannerandparserforthem.

3.1 SpecificationandImplementationStrategy 59
Real World
input−output
program text syntax tree
answer
Front End Interpreter
(a) Execution via interpreter
Real World
input−output
translated
program text syntax tree
program answer
Front End Compiler Interpreter
or Machine
(b) Execution via Compiler
Figure3.1 Blockdiagramsforalanguage-processingsystem
Parser generator systems are available for most major languages. If no
parsergeneratorisavailable,ornoneissuitablefortheapplication,onecan
choose to build a scanner and parserby hand. This process is described in
compiler textbooks. The parsing technology and associated grammars we
usearedesignedforsimplicityinthecontextofourveryspecializedneeds.
Another approach is to ignore the details of the concrete syntax and
to write our expressions as list structures, as we did for lambda-calculus
expressionswiththeprocedureparse-expressioninsection2.5andexer-
cise2.31.

60 3 Expressions
| Program ::=Expression |     |     |     |
| --------------------- | --- | --- | --- |
a-program (exp1)
Expression::=Number
const-exp (num)
| Expression::=-(Expression | , Expression)  |       |     |
| ------------------------- | -------------- | ----- | --- |
|                           | diff-exp (exp1 | exp2) |     |
| Expression::=zero?        | (Expression)   |       |     |
zero?-exp (exp1)
| Expression::=if | Expression then | Expression else | Expression |
| --------------- | --------------- | --------------- | ---------- |
|                 | if-exp (exp1    | exp2 exp3)      |            |
Expression::=Identifier
var-exp (var)
| Expression::=let | Identifier = | Expression in Expression |     |
| ---------------- | ------------ | ------------------------ | --- |
|                  | let-exp (var | exp1 body)               |     |
Figure3.2 SyntaxfortheLETlanguage
3.2 LET: ASimpleLanguage
Webeginbyspecifyingaverysimplelanguage,whichwecallLET,afterits
mostinterestingfeature.
3.2.1 SpecifyingtheSyntax
Figure3.2showsthesyntaxofoursimplelanguage. Inthislanguage,apro-
gram is just an expression. An expression is either an integer constant, a
differenceexpression,azero-testexpression,aconditionalexpression,avari-
able,oraletexpression.
Here is a simple expression in this language and its representation as
abstractsyntax.
| (scan&parse "-(55, | -(x,11))") |     |     |
| ------------------ | ---------- | --- | --- |
#(struct:a-program
#(struct:diff-exp
| #(struct:const-exp | 55) |     |     |
| ------------------ | --- | --- | --- |
#(struct:diff-exp
| #(struct:var-exp   | x)  |        |     |
| ------------------ | --- | ------ | --- |
| #(struct:const-exp |     | 11)))) |     |

| 3.2 LET:ASimpleLanguage |     |     |     |     | 61  |
| ----------------------- | --- | --- | --- | --- | --- |
3.2.2 SpecificationofValues
Animportantpartof the specification of anyprogramminglanguage isthe
setofvaluesthatthelanguagemanipulates. Eachlanguagehasatleasttwo
such sets: the expressed values and the denoted values. The expressed values
arethepossiblevaluesofexpressions,andthedenotedvaluesarethevalues
boundtovariables.
In the languages of this chapter, the expressed and denoted values will
| alwaysbethesame. | Theywillstartoutas |              |        |     |     |
| ---------------- | ------------------ | ------------ | ------ | --- | --- |
|                  |                    | =            | +      |     |     |
|                  |                    | ExpVal Int   | Bool   |     |     |
|                  |                    | DenVal = Int | + Bool |     |     |
Chapter 4 presents languages in which expressed and denoted values are
different.
In order to make use of this definition, we will need an interface for the
| datatypeofexpressedvalues. |     | Ourinterfacewillhavetheentries |     |     |     |
| -------------------------- | --- | ------------------------------ | --- | --- | --- |
→
| num-val | : Int | ExpVal |     |     |     |
| ------- | ----- | ------ | --- | --- | --- |
→
| bool-val    | : Bool   | ExpVal |     |     |     |
| ----------- | -------- | ------ | --- | --- | --- |
| expval->num | : ExpVal | → Int  |     |     |     |
→
| expval->bool | : ExpVal | Bool |     |     |     |
| ------------ | -------- | ---- | --- | --- | --- |
Weassumethatexpval->numandexpval->boolareundefinedwhen
givenanargumentthatisnotanumberoraboolean,respectively.
3.2.3 Environments
Ifwearegoingtoevaluateexpressionscontainingvariables,wewillneedto
knowthe valueassociatedwitheachvariable. We dothisbykeepingthose
valuesinanenvironment,asdefinedinsection2.2.
Anenvironmentisafunctionwhosedomainisafinitesetofvariablesand
whoserangeisthedenotedvalues.Weusesomeabbreviationswhenwriting
aboutenvironments.
• ρ rangesoverenvironments.
• []denotestheemptyenvironment.
| =           | ρ                  |                     |         | ρ).          |            |
| ----------- | ------------------ | ------------------- | ------- | ------------ | ---------- |
| • [var val] | denotes(extend-env |                     | var val |              |            |
| • [var =    | val , var =        | val ] ρ abbreviates | [var =  | val ]([var = | val ] ρ ), |
| 1           | 1 2                | 2                   | 1       | 1 2          | 2          |
etc.
| • [var =   | val , var = | val ,... ] denotestheenvironment |     | inwhichthe |     |
| ---------- | ----------- | -------------------------------- | --- | ---------- | --- |
| 1          | 1 2         | 2                                |     |            |     |
| valueofvar | isval       | ,etc.                            |     |            |     |
1 1

62 3 Expressions
Wewilloccasionallywrite downcomplicatedenvironmentsusinginden-
tationtoimprovereadability.Forexample,wemightwrite
[x=3]
[y=7]
[u=5]ρ
toabbreviate
| (extend-env | ’x 3 |        |     |     |
| ----------- | ---- | ------ | --- | --- |
| (extend-env | ’y   | 7      |     |     |
| (extend-env | ’u   | 5 ρ))) |     |     |
3.2.4 SpecifyingtheBehaviorofExpressions
Therearesixkindsofexpressionsinourlanguage: oneforeachproduction
withExpressionasitsleft-handside. Our interfacefor expressionswill con-
tainsevenprocedures: six constructorsandoneobserver. We useExpValto
denotethesetofexpressedvalues.
constructors:
: Int → Exp
const-exp
→
| zero?-exp | : Exp   | Exp         |     |     |
| --------- | ------- | ----------- | --- | --- |
|           | ×       | × →         |     |     |
| if-exp    | : Exp   | Exp Exp Exp |     |     |
| diff-exp  | : Exp × | Exp → Exp   |     |     |
→
| var-exp | : Var   | Exp             |     |     |
| ------- | ------- | --------------- | --- | --- |
|         | : Var × | Exp × Exp → Exp |     |     |
let-exp
observer:
|          | ×     | →          |     |     |
| -------- | ----- | ---------- | --- | --- |
| value-of | : Exp | Env ExpVal |     |     |
Before starting on an implementation, we write down a specification for
thebehavioroftheseprocedures.Followingtheinterpreterrecipe,weexpect
thatvalue-ofwill look atthe expression, determine whatkind of expres-
sionitis,andreturntheappropriatevalue.
| (value-of | (const-exp | n) ρ) | = (num-val   | n)   |
| --------- | ---------- | ----- | ------------ | ---- |
|           |            | ρ)    |              | ρ    |
| (value-of | (var-exp   | var)  | = (apply-env | var) |
ρ)
| (value-of | (diff-exp | exp exp | )   |     |
| --------- | --------- | ------- | --- | --- |
|           |           | 1       | 2   |     |
= (num-val
(-
ρ))
|     | (expval->num | (value-of | exp |     |
| --- | ------------ | --------- | --- | --- |
1
|     | (expval->num | (value-of | exp ρ)))) |     |
| --- | ------------ | --------- | --------- | --- |
2

3.2 LET:ASimpleLanguage 63
Thevalueofaconstantexpressioninanyenvironmentistheconstantval-
ue. The value of a variable reference in an environment is determined by
lookingupthevariableintheenvironment. Thevalueofadifferenceexpres-
sion in some environment is the difference between the value of the first
operand in that environment and the value of the second operand in that
environment. Ofcourse,tobeprecisewehavetomakesurethatthevalues
of the operands are numbers, and we have to make sure that value of the
resultisanumberrepresentedasanexpressedvalue.
Figure3.3showshowtheserulesworktogethertospecifythevalueofan
expression built by these constructors. In this and our other examples, we
write«exp»todenotetheASTforexpressionexp.Wealsowrite (cid:13) n (cid:14) inplace
of(num-val n),and (cid:16) val (cid:17) inplaceof(expval->num val). Wewillalso
(cid:16)(cid:13) (cid:14)(cid:17)
usethefactthat n =n.
Exercise3.1 [ (cid:3) ] Infigure3.3,listalltheplaceswhereweusedthefactthat(cid:16)(cid:13)n(cid:14)(cid:17)=n.
Exercise3.2 [ (cid:3)(cid:3) ] Giveanexpressedvalueval∈ExpValforwhich(cid:13)(cid:16)val(cid:17)(cid:14)(cid:3)=val.
3.2.5 SpecifyingtheBehaviorofPrograms
Inourlanguage,awholeprogramisjustanexpression. Inordertofindthe
value of such an expression, we need to specify the values of the free vari-
ables in the program. So the value of a program is just the value of that
expressionin a suitable initial environment. We choose our initial environ-
menttobe[i=1,v=5,x=10].
(value-of-program exp)
= (value-of exp [i=(cid:13)1(cid:14),v=(cid:13)5(cid:14),x=(cid:13)10(cid:14)])
3.2.6 SpecifyingConditionals
Thenextportionofthelanguageintroducesaninterfaceforbooleansinour
language. The language has one constructor of booleans, zero?, and one
observerofbooleans,theifexpression.
The value of a zero? expression is a true value if and only if the value
of its operand is zero. We can write this as a rule of inference like those in
definition 1.1.5. We use bool-val as a constructor to turn a boolean into
anexpressedvalue,andexpval->numasanextractortocheckwhetheran
expressedvalueisaninteger,andifso,toreturntheinteger.

64 3 Expressions
Letρ= [i=1,v=5,x=10].
(value-of
| <<-(-(x,3), | -(v,i))>> |     | = (cid:13)(- |     |
| ----------- | --------- | --- | ------------ | --- |
| ρ)          |           |     | 7            |     |
(-
| (cid:13)(-        |            |                     | (cid:16)(value-of | ρ)(cid:17)           |
| ----------------- | ---------- | ------------------- | ----------------- | -------------------- |
| =                 |            |                     | <<v>>             |                      |
| (cid:16)(value-of |            | ρ)(cid:17)          | (cid:16)(value-of | ρ)(cid:17)))(cid:14) |
|                   | <<-(x,3)>> |                     | <<i>>             |                      |
| (cid:16)(value-of | <<-(v,i)>> | ρ)(cid:17))(cid:14) |                   |                      |
= (cid:13)(-
| = (cid:13)(-      |            |                     | 7                 |                      |
| ----------------- | ---------- | ------------------- | ----------------- | -------------------- |
| (-                |            |                     | (-                |                      |
| (cid:16)(value-of |            | ρ)(cid:17)          |                   |                      |
|                   | <<x>>      |                     | 5                 |                      |
| (cid:16)(value-of |            | ρ)(cid:17))         | (cid:16)(value-of | ρ)(cid:17)))(cid:14) |
|                   | <<3>>      |                     | <<i>>             |                      |
| (cid:16)(value-of | <<-(v,i)>> | ρ)(cid:17))(cid:14) |                   |                      |
= (cid:13)(-
| = (cid:13)(-      |     |             | 7           |     |
| ----------------- | --- | ----------- | ----------- | --- |
| (-                |     |             | (-          |     |
| 10                |     |             | 5           |     |
| (cid:16)(value-of |     | ρ)(cid:17)) | 1))(cid:14) |     |
<<3>>
| (value-of | <<-(v,i)>> | ρ))(cid:14) |     |     |
| --------- | ---------- | ----------- | --- | --- |
= (cid:13)(-
| = (cid:13)(- |     |     | 7          |     |
| ------------ | --- | --- | ---------- | --- |
| (-           |     |     | 4)(cid:14) |     |
10
(cid:13)3(cid:14)
| 3)                |            |                     | =   |     |
| ----------------- | ---------- | ------------------- | --- | --- |
| (cid:16)(value-of | <<-(v,i)>> | ρ)(cid:17))(cid:14) |     |     |
= (cid:13)(-
7
| (cid:16)(value-of |     | ρ)(cid:17))(cid:14) |     |     |
| ----------------- | --- | ------------------- | --- | --- |
<<-(v,i)>>
Figure3.3 Asimplecalculationusingthespecification

3.2 LET:ASimpleLanguage 65
(value-of exp ρ ) = val
1 1
(val (cid:2) ue-of (zero?-exp exp 1 ) ρ)
(bool-val #t) if (expval->num val ) = 0
= 1
(bool-val #f) if (expval->num val ) (cid:3)= 0
1
Anifexpressionisanobserverofbooleanvalues.Todeterminethevalue
ofanifexpression(if-exp exp exp exp ),wemustfirstdeterminethe
1 2 3
valueofthesubexpressionexp . Ifthisvalueisatruevalue,thevalueofthe
1
entire if-exp should be the value of the subexpression exp ; otherwise it
2
should be the value of the subexpression exp . This is also easyto write as
3
aruleofinference. Weuseexpval->booltoextractthebooleanpartofan
expressedvalue,justasweusedexpval->numintheprecedingexample.
(value-of exp ρ ) = val
1 1
(val (cid:2) ue-of (if-exp exp 1 exp 2 exp 3 ) ρ)
(value-of exp ρ) if (expval->bool val ) = #t
= 2 1
(value-of exp ρ) if (expval->bool val ) = #f
3 1
Rules of inference like this make the intended behavior of any individ-
ual expression easy to specify, but they are not very good for displaying a
deduction. An antecedent like (value-of exp ρ ) = val denotes a sub-
1 1
computation,soacalculationshouldbeatree,muchliketheoneonpage5.
Unfortunately, such treescan be difficult to read. We therefore often recast
ourrulesasequations. We canthenusesubstitution of equalsforequalsto
displayacalculation.
Foranif-exp,theequationalspecificationis
(value-of (if-exp exp exp exp ) ρ)
1 2 3
= (if (expval->bool (value-of exp ρ))
1
(value-of exp ρ)
2
(value-of exp ρ))
3
Figure3.4showsasimplecalculationusingtheserules.
3.2.7 Specifyinglet
Nextweaddresstheproblemofcreatingnewvariablebindingswithalet
expression. We addto the interpretedlanguage a syntax in which the key-
word let isfollowed by a declaration, the keyword in, and the body. For
example,

66 3 Expressions
ρ=[x=(cid:13)33(cid:14),y=(cid:13)22(cid:14)].
Let
(value-of
| <<if | zero?(-(x,11)) |     |     | then -(y,2) | else | -(y,4)>> |
| ---- | -------------- | --- | --- | ----------- | ---- | -------- |
ρ)
ρ))
| = (if | (expval->bool |     | (value-of |     | <<zero?(-(x,11))>> |     |
| ----- | ------------- | --- | --------- | --- | ------------------ | --- |
ρ)
| (value-of |               | <<-(y,2)>> |           |     |      |     |
| --------- | ------------- | ---------- | --------- | --- | ---- | --- |
| (value-of |               | <<-(y,4)>> |           | ρ)) |      |     |
| = (if     | (expval->bool |            | (bool-val |     | #f)) |     |
| (value-of |               | <<-(y,2)>> |           | ρ)  |      |     |
ρ))
| (value-of |     | <<-(y,4)>> |     |     |     |     |
| --------- | --- | ---------- | --- | --- | --- | --- |
= (if #f
| (value-of |     | <<-(y,2)>> |     | ρ)  |     |     |
| --------- | --- | ---------- | --- | --- | --- | --- |
| (value-of |     | <<-(y,4)>> |     | ρ)) |     |     |
ρ)
| = (value-of |     | <<-(y,4)>> |     |     |     |     |
| ----------- | --- | ---------- | --- | --- | --- | --- |
(cid:13)18(cid:14)
=
|     | Figure3.4 |     | Asimplecalculationforaconditionalexpression |     |     |     |
| --- | --------- | --- | ------------------------------------------- | --- | --- | --- |
|     | let x     | = 5 |                                             |     |     |     |
|     | in -(x,3) |     |                                             |     |     |     |
Theletvariableisboundinthebody,muchasalambdavariableisbound
(seesection1.2.4).
The entire let form is an expression, as is its body, so let expressions
maybenested,asin
|     | let z  | = 5    |          |         |          |     |
| --- | ------ | ------ | -------- | ------- | -------- | --- |
|     | in let | x =    | 3        |         |          |     |
|     | in     | let y  | = -(x,1) |         | % here x | = 3 |
|     |        | in let | x =      | 4       |          |     |
|     |        | in     | -(z,     | -(x,y)) | % here x | = 4 |
In this example, the reference to x in the first difference expression refers
to the outer declaration, whereas the reference to x in the other difference
expression refers to the inner declaration, and thus the entire expression’s
valueis3.

3.2 LET:ASimpleLanguage 67
Theright-handsideoftheletisalsoanexpression,soitcanbearbitrarily
| complex. Forexample, |          |       |            |     |
| -------------------- | -------- | ----- | ---------- | --- |
| let                  | x = 7    |       |            |     |
| in                   | let y =  | 2     |            |     |
|                      | in let y | = let | x = -(x,1) |     |
in -(x,y)
|     | in -(-(x,8), |     | y)  |     |
| --- | ------------ | --- | --- | --- |
Herethexdeclaredonthethirdlineisboundto6,sothevalueofyis4,and
| thevalueoftheentireexpressionis(( |     |     | − 1) − | 4) =− 5. |
| --------------------------------- | --- | --- | ------ | -------- |
Wecanwritedownthespecificationasarule.
|     |             | (value-of | exp       | ρ ) = val  |
| --- | ----------- | --------- | --------- | ---------- |
|     |             |           | 1         | 1          |
|     | (value-of   | (let-exp  |           | body) ρ)   |
|     |             |           | var       | exp 1      |
|     | = (value-of |           | body [var | = val ] ρ) |
1
Asbefore,itisoftenmoreconvenienttorecastthisastheequation
ρ)
| (value-of | (let-exp | var | exp body) |     |
| --------- | -------- | --- | --------- | --- |
1
| = (value-of |      | [var=(value-of |     | ρ)]ρ) |
| ----------- | ---- | -------------- | --- | ----- |
|             | body |                |     | exp 1 |
ρ
Figure3.5showsanexample. There denotesanarbitraryenvironment.
0
3.2.8 ImplementingtheSpecificationofLET
Our next task is to implement this specification as a set of Scheme proce-
dures. OurimplementationusesSLLGENasafrontend,whichmeansthat
expressionswillberepresentedbyadatatypeliketheoneinfigure3.6. The
representation of expressed values in our implementation is shown in fig-
ure3.7. Thedatatypedeclarestheconstructorsnum-valandbool-valfor
convertingintegersandbooleanstoexpressedvalues. Wealsodefineextrac-
tors for converting from an expressed value back to either an integer or a
boolean. The extractors report an error if an expressed value is not of the
expectedkind.

68 3 Expressions
(value-of
| <<let x = | 7             |          |     |        |     |     |
| --------- | ------------- | -------- | --- | ------ | --- | --- |
| in let    | y = 2         |          |     |        |     |     |
| in let    | y = let x     | = -(x,1) | in  | -(x,y) |     |     |
| in        | -(-(x,8),y)>> |          |     |        |     |     |
ρ )
0
= (value-of
| <<let y                | = 2           |        |     |        |     |     |
| ---------------------- | ------------- | ------ | --- | ------ | --- | --- |
| in let                 | y = let x =   | -(x,1) | in  | -(x,y) |     |     |
| in                     | -(-(x,8),y)>> |        |     |        |     |     |
| [x=(cid:13)7(cid:14)]ρ | )             |        |     |        |     |     |
0
= (value-of
| <<let y | = let x = -(x,1) |     | in -(x,y) |     |     |     |
| ------- | ---------------- | --- | --------- | --- | --- | --- |
in -(-(x,8),y)>>
| [y=(cid:13)2(cid:14)][x=(cid:13)7(cid:14)]ρ | )   |     |     |     |     |     |
| ------------------------------------------- | --- | --- | --- | --- | --- | --- |
0
| Letρ =[y=(cid:13)2(cid:14)][x=(cid:13)7(cid:14)]ρ | .   |     |     |     |     |     |
| ------------------------------------------------- | --- | --- | --- | --- | --- | --- |
| 1                                                 | 0   |     |     |     |     |     |
= (value-of
<<-(-(x,8),y)>>
| [y=(value-of | <<let x | = -(x,1) |     | in -(x,y)>> | ρ )] |     |
| ------------ | ------- | -------- | --- | ----------- | ---- | --- |
1
ρ )
1
= (value-of
<<-(-(x,8),y)>>
ρ )]ρ
| [y=(value-of | <<-(x,2)>> | [x=(value-of |     | <<-(x,1)>> |     | )]  |
| ------------ | ---------- | ------------ | --- | ---------- | --- | --- |
| ρ            |            |              |     |            | 1   | 1   |
)
1
= (value-of
<<-(-(x,8),y)>>
[x=(cid:13)6(cid:14)]ρ
| [y=(value-of | <<-(x,2)>> |     |     | 1 )] |     |     |
| ------------ | ---------- | --- | --- | ---- | --- | --- |
ρ
)
1
= (value-of
<<-(-(x,8),y)>>
| [y=(cid:13)4(cid:14)]ρ | )   |     |     |     |     |     |
| ---------------------- | --- | --- | --- | --- | --- | --- |
1
| (cid:13)(- | 4)(cid:14) |     |     |     |     |     |
| ---------- | ---------- | --- | --- | --- | --- | --- |
= (- 7 8)
= (cid:13)-5(cid:14)
|     | Figure3.5 |     | Anexampleoflet |     |     |     |
| --- | --------- | --- | -------------- | --- | --- | --- |

3.2 LET:ASimpleLanguage 69
| (define-datatype | program | program? |
| ---------------- | ------- | -------- |
(a-program
| (exp1 expression?))) |            |             |
| -------------------- | ---------- | ----------- |
| (define-datatype     | expression | expression? |
(const-exp
(num number?))
(diff-exp
| (exp1 expression?)  |     |     |
| ------------------- | --- | --- |
| (exp2 expression?)) |     |     |
(zero?-exp
| (exp1 expression?)) |     |     |
| ------------------- | --- | --- |
(if-exp
| (exp1 expression?)  |     |     |
| ------------------- | --- | --- |
| (exp2 expression?)  |     |     |
| (exp3 expression?)) |     |     |
(var-exp
(var identifier?))
(let-exp
(var identifier?)
| (exp1 expression?)   |     |     |
| -------------------- | --- | --- |
| (body expression?))) |     |     |
Figure3.6 SyntaxdatatypesfortheLETlanguage
We can use any implementation of environments, provided that it meets
thespecificationinsection2.2.Theprocedureinit-envconstructsthespec-
ifiedinitialenvironmentusedbyvalue-of-program.
→
| init-env : () | Env |     |
| ------------- | --- | --- |
[i=(cid:13)1(cid:14),v=(cid:13)5(cid:14),x=(cid:13)10(cid:14)]
| usage: (init-env) | =   |     |
| ----------------- | --- | --- |
(define init-env
| (lambda () |     |     |
| ---------- | --- | --- |
(extend-env
| ’i (num-val | 1)  |     |
| ----------- | --- | --- |
(extend-env
| ’v (num-val | 5)  |     |
| ----------- | --- | --- |
(extend-env
| ’x (num-val | 10) |     |
| ----------- | --- | --- |
(empty-env))))))

70 3 Expressions
(define-datatype expval expval?
(num-val
(num number?))
(bool-val
(bool boolean?)))
expval->num : ExpVal → Int
(define expval->num
(lambda (val)
(cases expval val
(num-val (num) num)
(else (report-expval-extractor-error ’num val)))))
expval->bool : ExpVal → Bool
(define expval->bool
(lambda (val)
(cases expval val
(bool-val (bool) bool)
(else (report-expval-extractor-error ’bool val)))))
Figure3.7 ExpressedvaluesfortheLETlanguage
Nowwecanwritedowntheinterpreter,showninfigures3.8and3.9. The
mainprocedureisrun,whichtakesastring,parsesit,andhandstheresultto
value-of-program. Themostinterestingprocedureisvalue-of,which
takes an expression and an environment and uses the interpreter recipe
to calculate the answer required by the specification. In the listing below
we have inserted the relevant specification rules to show how the code for
value-ofcomesfromthespecification.
Inthefollowingexercises,andthroughoutthebook,thephrase“extendthelanguage
byadding...” means towritedownadditional rulesorequationstothe language
specification, and to implement the feature by adding or modifyingthe associated
interpreter.
(cid:3)
Exercise3.3 [ ]Whyissubtractionabetterchoicethanadditionforoursinglearith-
meticoperation?
(cid:3)
Exercise3.4 [ ]Writeoutthederivationoffigure3.4asaderivationtreeinthestyle
oftheoneonpage5.

3.2 LET:ASimpleLanguage 71
→
| run     | : String          | ExpVal |             |     |            |     |
| ------- | ----------------- | ------ | ----------- | --- | ---------- | --- |
| (define | run               |        |             |     |            |     |
| (lambda | (string)          |        |             |     |            |     |
|         | (value-of-program |        | (scan&parse |     | string)))) |     |
: →
| value-of-program |                   | Program |            | ExpVal          |         |     |
| ---------------- | ----------------- | ------- | ---------- | --------------- | ------- | --- |
| (define          | value-of-program  |         |            |                 |         |     |
| (lambda          | (pgm)             |         |            |                 |         |     |
|                  | (cases program    |         | pgm        |                 |         |     |
|                  | (a-program        | (exp1)  |            |                 |         |     |
|                  | (value-of         |         | exp1       | (init-env)))))) |         |     |
|                  |                   | ×       | →          |                 |         |     |
| value-of         | : Exp             | Env     | ExpVal     |                 |         |     |
| (define          | value-of          |         |            |                 |         |     |
| (lambda          | (exp              | env)    |            |                 |         |     |
|                  | (cases expression |         | exp        |                 |         |     |
|                  | (value-of         |         | (const-exp |                 | n) ρ) = |     |
n
|     | (const-exp | (num)             |              | (num-val   | num))             |                     |
| --- | ---------- | ----------------- | ------------ | ---------- | ----------------- | ------------------- |
|     | (value-of  |                   | (var-exp     | var)       | ρ) = (apply-env   | ρ var)              |
|     | (var-exp   | (var)             | (apply-env   |            | env var))         |                     |
|     | (value-of  |                   | (diff-exp    |            | ) ρ)=             |                     |
|     |            |                   |              | exp        | 1 exp 2           |                     |
|     | (cid:13)(- | (cid:16)(value-of |              | ρ)(cid:17) | (cid:16)(value-of | ρ)(cid:17))(cid:14) |
|     |            |                   |              | exp 1      | exp               | 2                   |
|     | (diff-exp  | (exp1             | exp2)        |            |                   |                     |
|     | (let       | ((val1            | (value-of    |            | exp1 env))        |                     |
|     |            | (val2             | (value-of    |            | exp2 env)))       |                     |
|     | (let       | ((num1            | (expval->num |            | val1))            |                     |
|     |            | (num2             | (expval->num |            | val2)))           |                     |
(num-val
(- num1 num2)))))
|     | Figure3.8 |     | InterpreterfortheLETlanguage |     |     |     |
| --- | --------- | --- | ---------------------------- | --- | --- | --- |

72 3 Expressions
(value-of exp ρ)=val
1 1
(val(cid:2)ue-of (zero?-exp exp
1
) ρ)
(bool-val #t) if (expval->num val )=0
= 1
(bool-val #f) if (expval->num val )(cid:3)=0
1
(zero?-exp (exp1)
(let ((val1 (value-of exp1 env)))
(let ((num1 (expval->num val1)))
(if (zero? num1)
(bool-val #t)
(bool-val #f)))))
(value-of exp ρ)=val
1 1
(val(cid:2)ue-of (if-exp exp
1
exp
2
exp
3
) ρ)
(value-of exp ρ) if (expval->bool val )=#t
= 2 1
(value-of exp ρ) if (expval->bool val )=#f
3 1
(if-exp (exp1 exp2 exp3)
(let ((val1 (value-of exp1 env)))
(if (expval->bool val1)
(value-of exp2 env)
(value-of exp3 env))))
(value-of exp ρ)=val
1 1
(value-of (let-exp var exp body) ρ)
1
= (value-of body [var=val ]ρ)
1
(let-exp (var exp1 body)
(let ((val1 (value-of exp1 env)))
(value-of body
(extend-env var val1 env)))))))
Figure3.9 InterpreterfortheLETlanguage,continued
(cid:3)
Exercise3.5 [ ]Writeoutthederivationoffigure3.5asaderivationtreeinthestyle
oftheoneonpage5.
(cid:3)
Exercise3.6 [ ] Extendthelanguagebyaddinganewoperatorminusthattakesone
argument,n,andreturns−n. Forexample,thevalueofminus(-(minus(5),9))
shouldbe14.
(cid:3)
Exercise3.7 [ ] Extend the language by adding operators for addition, multiplica-
tion,andintegerquotient.

3.2 LET:ASimpleLanguage 73
(cid:3)
Exercise3.8 [ ]Addanumericequalitypredicateequal?andnumericorderpredi-
catesgreater?andless?tothesetofoperationsinthedefinedlanguage.
(cid:3)(cid:3) cons,
| Exercise3.9 | [ ]Add | list processing | operations | to  | the language, | including |
| ----------- | ------ | --------------- | ---------- | --- | ------------- | --------- |
car, cdr, null? and emptylist. Alistshould be able to contain any expressed
value,includinganotherlist. Givethedefinitionsoftheexpressedanddenotedval-
uesofthelanguage,asinsection3.2.2.Forexample,
|     | let x = 4  |     |     |     |     |     |
| --- | ---------- | --- | --- | --- | --- | --- |
|     | in cons(x, |     |     |     |     |     |
cons(cons(-(x,1),
emptylist),
emptylist))
| shouldreturnanexpressedvaluethatrepresentsthelist(4 |     |     |     |     | (3)). |     |
| --------------------------------------------------- | --- | --- | --- | --- | ----- | --- |
(cid:3)(cid:3)
Exercise3.10 [ ] Add an operationlist to the language. This operationshould
takeanynumberofarguments,andreturnanexpressedvaluecontainingthelistof
theirvalues.Forexample,
|                                                     | let x = 4  |         |         |     |     |     |
| --------------------------------------------------- | ---------- | ------- | ------- | --- | --- | --- |
|                                                     | in list(x, | -(x,1), | -(x,3)) |     |     |     |
| shouldreturnanexpressedvaluethatrepresentsthelist(4 |            |         |         |     | 3   | 1). |
(cid:3)
Exercise3.11 [ ]Inareallanguage,onemighthavemanyoperatorssuchasthosein
theprecedingexercises.Rearrangethecodeintheinterpretersothatitiseasytoadd
newoperators.
(cid:3)
Exercise3.12 [ ]Addtothedefinedlanguageafacilitythataddsacondexpression.
Usethegrammar
Expression}∗
|     | Expression::=cond |     | {Expression | ==> |     | end |
| --- | ----------------- | --- | ----------- | --- | --- | --- |
Inthisexpression,theexpressionsontheleft-handsidesofthe==>’sareevaluatedin
orderuntiloneofthemreturnsatruevalue. Thenthevalueoftheentireexpression
isthevalueofthecorrespondingright-handexpression.Ifnoneofthetestssucceeds,
theexpressionshouldreportanerror.
(cid:3)
Exercise3.13 [ ] Change the values of the language so that integers are the only
expressedvalues.Modifyifsothatthevalue0istreatedasfalseandallothervalues
aretreatedastrue.Modifythepredicatesaccordingly.
Exercise3.14 [ (cid:3)(cid:3) ] As an alternative to the preceding exercise, add a new nonter-
minal Bool-exp of boolean expressionsto the language. Change the production for
conditionalexpressionstosay
|     | Expression::=if |          | then |            | else |            |
| --- | --------------- | -------- | ---- | ---------- | ---- | ---------- |
|     |                 | Bool-exp |      | Expression |      | Expression |
Write suitable productions for Bool-exp and implement value-of-bool-exp.
Wheredothepredicatesofexercise3.8windupinthisorganization?

74 3 Expressions
(cid:3)
Exercise3.15 [ ] Extendthelanguagebyaddinganewoperationprintthattakes
oneargument,printsit,andreturnstheinteger1. Whyisthisoperationnotexpress-
ibleinourspecificationframework?
(cid:3)(cid:3)
Exercise3.16 [ ]Extendthelanguagesothataletdeclarationcandeclareanarbi-
trarynumberofvariables,usingthegrammar
| Expression::=let |     | {Identifier=Expression}∗ |     |
| ---------------- | --- | ------------------------ | --- |
in Expression
AsinScheme’slet,eachoftheright-handsidesisevaluatedinthecurrentenviron-
ment, and the body is evaluated with each new variable bound to the value of its
associatedright-handside.Forexample,
| let x  | = 30       |     |     |
| ------ | ---------- | --- | --- |
| in let | x = -(x,1) |     |     |
|        | y = -(x,2) |     |     |
in -(x,y)
shouldevaluateto1.
| (cid:3)(cid:3) |     | let∗ |     |
| -------------- | --- | ---- | --- |
Exercise3.17 [ ] Extend the language with a expression that works like
Scheme’slet∗
,sothat
| let x | = 30 |     |     |
| ----- | ---- | --- | --- |
let∗
| in  | x = -(x,1) | y = -(x,2) |     |
| --- | ---------- | ---------- | --- |
in -(x,y)
shouldevaluateto2.
(cid:3)(cid:3)
| Exercise3.18 [      | ] Addanexpressiontothedefinedlanguage: |                            |               |
| ------------------- | -------------------------------------- | -------------------------- | ------------- |
| Expression::=unpack |                                        | {Identifier}∗ = Expression | in Expression |
sothatunpack x y z = lst in ...bindsx,y,andztotheelementsoflstif
lstisalistofexactlythreeelements,andreportsanerrorotherwise. Forexample,
thevalueof
| let u     | = 7                             |     |     |
| --------- | ------------------------------- | --- | --- |
| in unpack | x y = cons(u,cons(3,emptylist)) |     |     |
in -(x,y)
shouldbe4.
| 3.3 PROC:ALanguage | with | Procedures |     |
| ------------------ | ---- | ---------- | --- |
Sofarourlanguagehasonlytheoperationsthatwereincludedintheoriginal
language. For our interpreted language to be at all useful, we must allow
newprocedurestobecreated.WecallthenewlanguagePROC.

3.3 PROC:ALanguagewithProcedures 75
WewillfollowthedesignofScheme,andletproceduresbeexpressedval-
uesinourlanguage,sothat
= + +
ExpVal Int Bool Proc
= + +
DenVal Int Bool Proc
whereProcisasetofvaluesrepresentingprocedures. We willthinkofProc
asanabstractdatatype. Weconsideritsinterfaceandspecificationbelow.
Wewillalsoneedsyntaxforprocedurecreationandcalling. Thisisgiven
bytheproductions
Expression::=proc (Identifier) Expression
proc-exp (var body)
Expression::=(Expression Expression)
call-exp (rator rand)
In (proc-exp var body), the variable var is the bound variable or formal
parameter. Inaprocedurecall(call-exp exp exp ), the expressionexp
1 2 1
is the operator and exp is the operand or actual parameter. We use the word
2
argumenttorefertothevalueofanactualparameter.
Herearetwosimpleprogramsinthislanguage.
let f = proc (x) -(x,11)
in (f (f 77))
(proc (f) (f (f 77))
proc (x) -(x,11))
Thefirstprogramcreatesaprocedurethatsubtracts11fromitsargument.
Itcallstheresultingproceduref,andthenappliesftwiceto77,yieldingthe
answer55. Thesecondprogramcreatesaprocedurethattakesitsargument
and applies it twice to 77. The program then applies this procedure to the
subtract-11procedure.Theresultisagain55.
WenowturntothedatatypeProc. Itsinterfaceconsistsoftheconstructor
procedure, which tells how to build a procedure value, and the observer
apply-procedure,whichtellshowtoapplyaprocedurevalue.
Ournexttaskistodeterminewhatinformationmustbeincludedinavalue
representing a procedure. To do this, we consider what happens when we
writeaprocexpressioninanarbitrarypositioninourprogram.
The lexical scope rule tells us that when a procedureis applied, its body
isevaluatedin an environmentthat binds the formalparameterof the pro-
ceduretotheargumentofthecall. Variablesoccurringfreeintheprocedure
shouldalsoobeythelexicalbindingrule. Considertheexpression

76 3 Expressions
| let    | x = 200  |          |        |        |     |
| ------ | -------- | -------- | ------ | ------ | --- |
| in let | f = proc | (z)      | -(z,x) |        |     |
| in     | let x    | = 100    |        |        |     |
|        | in let   | g = proc | (z)    | -(z,x) |     |
|        | in       | -((f 1), | (g 1)) |        |     |
Hereweevaluatetheexpressionproc (z) -(z,x)twice. Thefirsttime
wedoit,xisboundto200,sobythelexicalscoperule,theresultisaproce-
durethat subtracts200from its argument. We name this proceduref. The
second time we do it, x is bound to 100, so the resulting procedureshould
| subtract100fromitsargument. |     |     | Wenamethisprocedureg. |     |     |
| --------------------------- | --- | --- | --------------------- | --- | --- |
These two procedures, created from identical expressions, must behave
differently. We conclude that the value of a proc expression must depend
in some way on the environment in which it is evaluated. Therefore the
constructorproceduremusttakethreearguments:
theboundvariable,the
| body,andtheenvironment. |           | Thespecificationforaprocexpressionis |           |     |     |
| ----------------------- | --------- | ------------------------------------ | --------- | --- | --- |
| (value-of               | (proc-exp |                                      | var body) | ρ)  |     |
ρ))
| = (proc-val | (procedure |     | var | body |     |
| ----------- | ---------- | --- | --- | ---- | --- |
where proc-val is a constructor, like bool-val or num-val, that builds
anexpressedvaluefromaProc.
At a procedure call, we want to find the value of the operator and the
operand. Ifthevalueoftheoperatorisaproc-val,thenwewanttoapply
ittothevalueoftheoperand.
ρ)
| (value-of | (call-exp |               | rator rand) |           |            |
| --------- | --------- | ------------- | ----------- | --------- | ---------- |
| = (let    | ((proc    | (expval->proc |             | (value-of | rator ρ))) |
ρ)))
|                  | (arg (value-of |     | rand |       |     |
| ---------------- | -------------- | --- | ---- | ----- | --- |
| (apply-procedure |                |     | proc | arg)) |     |
Herewerelyonatesterexpval->proc,likeexpval->num,totestwhether
thevalueof (value-of rator ρ),anexpressedvalue,wasconstructed by
proc-val,andifsotoextracttheunderlyingprocedure.
Last,weconsiderwhathappenswhenapply-procedureisinvoked. As
wehaveseen,thelexicalscoperuletellsusthatwhenaprocedureisapplied,
itsbodyisevaluatedinanenvironment thatbindsthe formalparameterof
the procedureto the argumentof the call. Furthermoreany other variables
must have the same valuesthey had at procedure-creationtime. Therefore
theseproceduresshouldsatisfythecondition
| (apply-procedure |      | (procedure  |     |          | ρ) val) |
| ---------------- | ---- | ----------- | --- | -------- | ------- |
|                  |      |             |     | var body |         |
| = (value-of      | body | [var=val]ρ) |     |          |         |

3.3 PROC:ALanguagewithProcedures 77
3.3.1 AnExample
Let’sdoanexampletoshowhowthepiecesofthespecificationfittogether.
Thisisacalculationusingthespecification,nottheimplementation,sincewe
ρ
havenotyetwrittendownthe implementation ofprocedures. Let beany
environment.
(value-of
| <<let x = 200 |                 |            |
| ------------- | --------------- | ---------- |
| in let f =    | proc (z) -(z,x) |            |
| in let        | x = 100         |            |
| in let        | g = proc        | (z) -(z,x) |
| in            | -((f 1),        | (g 1))>>   |
ρ)
= (value-of
| <<let f = | proc (z) -(z,x) |            |
| --------- | --------------- | ---------- |
| in let x  | = 100           |            |
| in let    | g = proc        | (z) -(z,x) |
| in        | -((f 1), (g     | 1))>>      |
[x=(cid:13)200(cid:14)]ρ)
= (value-of
| <<let x = | 100          |        |
| --------- | ------------ | ------ |
| in let g  | = proc (z)   | -(z,x) |
| in -((f   | 1), (g 1))>> |        |
[x=(cid:13)200(cid:14)]ρ))]
| [f=(proc-val | (procedure | z <<-(z,x)>> |
| ------------ | ---------- | ------------ |
[x=(cid:13)200(cid:14)]ρ)
= (value-of
| <<let g = | proc (z) -(z,x) |     |
| --------- | --------------- | --- |
| in -((f   | 1), (g 1))>>    |     |
[x=(cid:13)100(cid:14)]
[x=(cid:13)200(cid:14)]ρ))]
| [f=(proc-val | (procedure | z <<-(z,x)>> |
| ------------ | ---------- | ------------ |
[x=(cid:13)200(cid:14)]ρ)

78 3 Expressions
= (value-of
|     | <<-((f 1),   | (g 1))>>   |              |     |
| --- | ------------ | ---------- | ------------ | --- |
|     | [g=(proc-val | (procedure | z <<-(z,x)>> |     |
[x=(cid:13)100(cid:14)][f=...][x=(cid:13)200(cid:14)]ρ))]
[x=(cid:13)100(cid:14)]
|     | [f=(proc-val | (procedure | z <<-(z,x)>> | [x=(cid:13)200(cid:14)]ρ))] |
| --- | ------------ | ---------- | ------------ | --------------------------- |
[x=(cid:13)200(cid:14)]ρ)
= (cid:13)(-
|     | (value-of    | <<(f 1)>>  |              |     |
| --- | ------------ | ---------- | ------------ | --- |
|     | [g=(proc-val | (procedure | z <<-(z,x)>> |     |
[x=(cid:13)100(cid:14)][f=...][x=(cid:13)200(cid:14)]ρ))]
[x=(cid:13)100(cid:14)]
[x=(cid:13)200(cid:14)]ρ))]
|     | [f=(proc-val | (procedure | z <<-(z,x)>> |     |
| --- | ------------ | ---------- | ------------ | --- |
[x=(cid:13)200(cid:14)]ρ)
|     | (value-of    | <<(g 1)>>  |              |     |
| --- | ------------ | ---------- | ------------ | --- |
|     | [g=(proc-val | (procedure | z <<-(z,x)>> |     |
[x=(cid:13)100(cid:14)][f=...][x=(cid:13)200(cid:14)]ρ))]
[x=(cid:13)100(cid:14)]
[x=(cid:13)200(cid:14)]ρ))]
|     | [f=(proc-val | (procedure | z <<-(z,x)>> |     |
| --- | ------------ | ---------- | ------------ | --- |
[x=(cid:13)200(cid:14)]ρ))(cid:14)
= (cid:13)(-
(apply-procedure
|     | (procedure | z <<-(z,x)>> | [x=(cid:13)200(cid:14)]ρ) |     |
| --- | ---------- | ------------ | ------------------------- | --- |
(cid:13)1(cid:14))
(apply-procedure
|     | (procedure | z <<-(z,x)>> | [x=(cid:13)100(cid:14)][f=...][x=(cid:13)200(cid:14)]ρ) |     |
| --- | ---------- | ------------ | ------------------------------------------------------- | --- |
(cid:13)1(cid:14)))(cid:14)
= (cid:13)(-
[z=(cid:13)1(cid:14)][x=(cid:13)200(cid:14)]ρ)
|     | (value-of | <<-(z,x)>> |     |     |
| --- | --------- | ---------- | --- | --- |
[z=(cid:13)1(cid:14)][x=(cid:13)100(cid:14)][f=...][x=(cid:13)200(cid:14)]ρ))(cid:14)
|              | (value-of         | <<-(z,x)>> |     |     |
| ------------ | ----------------- | ---------- | --- | --- |
| = (cid:13)(- | -199 -99)(cid:14) |            |     |     |
= (cid:13)-100(cid:14)
Herefisboundtoaprocedurethatsubtracts200fromitsargument,and
gisboundtoaprocedurethatsubtracts100fromitsargument,sothevalue
| of(f | 1)is − 199andthevalueof(g |     | 1)is − 99. |     |
| ---- | ------------------------- | --- | ---------- | --- |

3.3 PROC:ALanguagewithProcedures 79
3.3.2 RepresentingProcedures
Accordingtotherecipedescribedinsection2.2.3,wecanemployaprocedu-
ralrepresentationforproceduresbytheiractionunderapply-procedure.
Todothiswedefineproceduretohaveavaluethatisanimplementation-
language procedure that expects an argument, and returns the value
requiredbythespecification
(apply-procedure (procedure var body ρ) val)
= (value-of body (extend-env var val ρ))
Thereforetheentireimplementationis
proc? : SchemeVal → Bool
(define proc?
(lambda (val)
(procedure? val)))
procedure : Var × Exp × Env → Proc
(define procedure
(lambda (var body env)
(lambda (val)
(value-of body (extend-env var val env)))))
apply-procedure : Proc × ExpVal → ExpVal
(define apply-procedure
(lambda (proc1 val)
(proc1 val)))
The function proc?, as defined here, is somewhat inaccurate, since not
everySchemeprocedureisapossibleprocedureinourlanguage. Weneedit
onlyfordefiningthedatatypeexpval.
Alternatively,wecoulduseadatastructurerepresentationlikethatofsec-
tion2.2.2.
proc? : SchemeVal → Bool
procedure : Var × Exp × Env → Proc
(define-datatype proc proc?
(procedure
(var identifier?)
(body expression?)
(saved-env environment?)))
apply-procedure : Proc × ExpVal → ExpVal
(define apply-procedure
(lambda (proc1 val)
(cases proc proc1
(procedure (var body saved-env)
(value-of body (extend-env var val saved-env))))))

80 3 Expressions
These data structures are often called closures, because they are self-
contained: they contain everything the procedure needs in order to be
applied.Wesometimessaytheprocedureisclosedoverorclosedinitscreation
environment.
Eachoftheseimplementationsevidentlysatisfiesthespecificationforthe
procedureinterface.
Ineitherimplementation,weaddanalternativetothedatatypeexpval
| (define-datatype |     | expval | expval? |     |     |
| ---------------- | --- | ------ | ------- | --- | --- |
(num-val
(num number?))
(bool-val
(bool boolean?))
(proc-val
(proc proc?)))
andweneedtoaddtwonewclausestovalue-of
| (proc-exp |                  | (var body)     |       |           |              |
| --------- | ---------------- | -------------- | ----- | --------- | ------------ |
|           | (proc-val        | (procedure     | var   | body      | env)))       |
| (call-exp |                  | (rator         | rand) |           |              |
|           | (let ((proc      | (expval->proc  |       | (value-of | rator env))) |
|           |                  | (arg (value-of | rand  | env)))    |              |
|           | (apply-procedure |                | proc  | arg)))    |              |
Reminder:besuretowritedownspecificationsforeachlanguageextension. Seethe
noteonpage70.
Exercise3.19 [ (cid:3) ]Inmanylanguages,proceduresmustbecreatedandnamedatthe
sametime.Modifythelanguageofthissectiontohavethispropertybyreplacingthe
procexpressionwithaletprocexpression.
(cid:3)
Exercise3.20 [ ]InPROC,procedureshaveonlyoneargument,butonecangetthe
effectofmultipleargumentproceduresbyusingproceduresthatreturnotherproce-
dures.Forexample,onemightwritecodelike
| let    | f = proc | (x) proc | (y) ... |     |     |
| ------ | -------- | -------- | ------- | --- | --- |
| in ((f | 3)       | 4)       |         |     |     |
ThistrickiscalledCurrying,andtheprocedureissaidtobeCurried. WriteaCurried
procedurethat takestwo argumentsand returnstheirsum. Youcanwrite x+y in
ourlanguagebywriting−(x,−(0,y)).
(cid:3)(cid:3)
Exercise3.21 [ ] Extend the language of this section to include procedures with
multipleargumentsandcallswithmultipleoperands,assuggestedbythegrammar
|     | Expression::=proc        |     | ({Identifier}∗(,)) |                | Expression |
| --- | ------------------------ | --- | ------------------ | -------------- | ---------- |
|     | Expression::=(Expression |     |                    | {Expression}∗) |            |

3.3 PROC:ALanguagewithProcedures 81
(cid:3)(cid:3)(cid:3)
Exercise3.22 [ ] The concrete syntax of this section uses different syntax for a
built-in operation, such as difference, from a procedure call. Modify the concrete
syntaxsothattheuserofthislanguageneednotknowwhichoperationsarebuilt-in
andwhicharedefinedprocedures. Thisexercisemayrangefromveryeasytohard,
dependingontheparsingtechnologybeingused.
(cid:3)(cid:3)
Exercise3.23 [ ]WhatisthevalueofthefollowingPROCprogram?
| let makemult | = proc (maker) |     |     |     |     |
| ------------ | -------------- | --- | --- | --- | --- |
proc (x)
if zero?(x)
|               | then       | 0          |        |           |     |
| ------------- | ---------- | ---------- | ------ | --------- | --- |
|               | else       | -(((maker  | maker) | -(x,1)),  | -4) |
| in let times4 | = proc (x) | ((makemult |        | makemult) | x)  |
| in (times4    | 3)         |            |        |           |     |
UsethetricksofthisprogramtowriteaprocedureforfactorialinPROC.Asahint,
rememberthatyoucanuseCurrying(exercise3.20)todefineatwo-argumentproce-
duretimes.
(cid:3)(cid:3)
Exercise3.24 [ ] Usethetricksoftheprogramabovetowritethepairofmutually
recursiveprocedures,oddandeven,asinexercise3.32.
(cid:3)
Exercise3.25 [ ]Thetricksofthepreviousexercisescanbegeneralizedtoshowthat
wecandefineanyrecursiveprocedureinPROC.Considerthefollowingbitofcode:
| let makerec       | = proc (f) |          |         |        |     |
| ----------------- | ---------- | -------- | ------- | ------ | --- |
|                   | let d =    | proc (x) |         |        |     |
|                   |            | proc     | (z) ((f | (x x)) | z)  |
|                   | in proc    | (n) ((f  | (d d))  | n)     |     |
| in let maketimes4 | = proc     | (f)      |         |        |     |
|                   | proc       | (x)      |         |        |     |
if zero?(x)
then 0
|               |            | else -((f   | -(x,1)), | -4) |     |
| ------------- | ---------- | ----------- | -------- | --- | --- |
| in let times4 | = (makerec | maketimes4) |          |     |     |
| in (times4    | 3)         |             |          |     |     |
Showthatitreturns12.
(cid:3)(cid:3)
Exercise3.26 [ ]Inourdata-structurerepresentationofprocedures,wehavekept
theentireenvironmentintheclosure. Butofcourseallweneedarethebindingsfor
the free variables. Modify the representation of proceduresto retain only the free
variables.
(cid:3) Addanewkindofprocedurecalledatraceproctothelanguage.
Exercise3.27 [ ]
A traceproc works exactly like a proc, except that it prints a trace message on
entryandonexit.

82 3 Expressions
(cid:3)(cid:3)
Exercise3.28 [ ] Dynamic binding(or dynamic scoping) is an alternative designfor
procedures,inwhichtheprocedurebodyisevaluatedinanenvironmentobtainedby
extendingtheenvironmentatthepointofcall.Forexamplein
|     |     | let a = 3  |          |        |     |     |
| --- | --- | ---------- | -------- | ------ | --- | --- |
|     |     | in let p = | proc (x) | -(x,a) |     |     |
|     |     | a =        | 5        |        |     |     |
|     |     | in -(a,(p  | 2))      |        |     |     |
theaintheprocedurebodywouldbeboundto5,not3. Modifythelanguagetouse
dynamicbinding. Dothis twice, once usingaproceduralrepresentationforproce-
dures,andonceusingadata-structurerepresentation.
(cid:3)(cid:3)
Exercise3.29 [ ] Unfortunately,programsthatusedynamicbindingmaybeexcep-
tionally difficult to understand. For example, under lexical binding, consistently
renamingtheboundvariablesofaprocedurecanneverchangethebehaviorofapro-
gram: wecanevenremoveallvariablesandreplacethembytheirlexicaladdresses,
asinsection3.6.Butunderdynamicbinding,thistransformationisunsafe.
Forexample,underdynamicbinding,theprocedureproc (z) areturnsthevalue
ofthevariableainitscaller’senvironment.Thus,theprogram
|     |     | let a = 3  |          |           |     |     |
| --- | --- | ---------- | -------- | --------- | --- | --- |
|     |     | in let p = | proc (z) | a         |     |     |
|     |     | in let f   | = proc   | (x) (p 0) |     |     |
|     |     | in let     | a = 5    |           |     |     |
|     |     | in         | (f 2)    |           |     |     |
returns5,sincea’svalueatthecallsiteis5.Whatiff’sformalparameterwerea?
| 3.4 | LETREC:A | Languagewith |     | Recursive | Procedures |     |
| --- | -------- | ------------ | --- | --------- | ---------- | --- |
We now define a new language LETREC, which adds recursion to our lan-
guage. Sinceourlanguagehasonlyone-argumentprocedures,wemakeour
life simpler by having our letrec expressions declare only a single one-
argumentprocedure,forexample
|     |     | letrec double(x) |          |      |                  |              |
| --- | --- | ---------------- | -------- | ---- | ---------------- | ------------ |
|     |     | = if             | zero?(x) | then | 0 else -((double | -(x,1)), -2) |
|     |     | in (double       | 6)       |      |                  |              |
The left-hand side of a recursive declaration is the name of the recursive
procedureanditsboundvariable.Totherightofthe=istheprocedurebody.
Theproductionforthisis
|     | Expression::=letrec |            | Identifier | (Identifier)  | = Expression | in Expression |
| --- | ------------------- | ---------- | ---------- | ------------- | ------------ | ------------- |
|     |                     | letrec-exp |            | (p-name b-var | p-body       | letrec-body)  |

| 3.4 LETREC:ALanguagewithRecursiveProcedures |     |     |     |     | 83  |
| ------------------------------------------- | --- | --- | --- | --- | --- |
Thevalueofaletrecexpressionisthevalueofthebodyinanenviron-
mentthathasthedesiredbehavior:
(value-of
| (letrec-exp | proc-name | bound-var | proc-body | letrec-body) |     |
| ----------- | --------- | --------- | --------- | ------------ | --- |
ρ)
= (value-of
letrec-body
| (extend-env-rec |     |           |           | ρ))       |     |
| --------------- | --- | --------- | --------- | --------- | --- |
|                 |     | proc-name | bound-var | proc-body |     |
Here we have added a new procedure extend-env-rec to the environ-
mentinterface. Butwestillneedtoanswerthequestion: Whatisthedesired
ρ)?
| behaviorof(extend-env-rec |     | proc-name | bound-var | proc-body |     |
| ------------------------- | --- | --------- | --------- | --------- | --- |
ρ
Wespecifythebehaviorofthisenvironmentasfollows: Let 1 betheenvi-
ronment produced by (extend-env-rec proc-name bound-var proc-body
| ρ).Thenwhatshould(apply-env |     | ρ   |     |     |     |
| --------------------------- | --- | --- | --- | --- | --- |
var)return?
1
ρ
1. If the variable var is the same as proc-name, then (apply-env var)
1
shouldproduceaclosurewhoseboundvariableisbound-var,whosebody
isproc-body,andwithanenvironmentinwhichproc-nameisboundtothis
ρ
procedure. But we alreadyhave such an environment, namely itself!
1
So
| (apply-env | ρ proc-name) |     |     |     |     |
| ---------- | ------------ | --- | --- | --- | --- |
1
ρ
| = (proc-val | (procedure | bound-var | proc-body | ))  |     |
| ----------- | ---------- | --------- | --------- | --- | --- |
1
2. Ifvarisnotthesameasproc-name,then
|     | (apply-env | ρ var)=(apply-env |     | ρ var) |     |
| --- | ---------- | ----------------- | --- | ------ | --- |
1
Figures3.10and3.11showanexample.Thereinthelastlineoffigure3.11,
therecursivecalltodoublefindstheoriginaldoubleprocedure,asdesired.
extend-env-rec
| We can | implement |     | in any | way that satisfies | these |
| ------ | --------- | --- | ------ | ------------------ | ----- |
requirements. We’ll doit here for the abstract-syntaxrepresentation. Some
otherimplementationstrategiesarediscussedintheexercises.
In an abstract-syntax representation, we add a new variant for an
extend-env-rec in figure 3.12. The env on the next-to-last line of
| apply-envcorrespondsto |     | ρ   |     |     |     |
| ---------------------- | --- | --- | --- | --- | --- |
1 inthediscussionabove.
(cid:3)
Exercise3.30 [ ]Whatisthepurposeofthecalltoproc-valonthenext-to-lastline
ofapply-env?
(cid:3)
Exercise3.31 [ ]Extendthe languageabove to allowthe declarationofarecursive
procedureofpossiblymanyarguments,asinexercise3.21.

84 3 Expressions
(value-of <<letrec double(x) = if zero?(x)
then 0
else -((double -(x,1)), -2)
in (double 6)>> ρ )
0
= (value-of <<(double 6)>>
(extend-env-rec double x <<if zero?(x) ...>> ρ ))
0
= (apply-procedure
(value-of <<double>> (extend-env-rec double x
<<if zero?(x) ...>> ρ ))
0
(value-of <<6>> (extend-env-rec double x
<<if zero?(x) ...>> ρ )))
0
= (apply-procedure
(procedure x <<if zero?(x) ...>>
(extend-env-rec double x <<if zero?(x) ...>> ρ ))
0
(cid:13)6(cid:14))
= (value-of
<<if zero?(x) ...>>
[x=(cid:13)6(cid:14)](extend-env-rec
double x <<if zero?(x) ...>> ρ ))
0
...
= (-
(value-of
<<(double -(x,1))>>
[x=(cid:13)6(cid:14)](extend-env-rec
double x <<if zero?(x) ...>> ρ ))
0
-2)
Figure3.10 Acalculationwithextend-env-rec
(cid:3)(cid:3)
Exercise3.32 [ ]Extendthelanguageabovetoallowthedeclarationofanynumber
ofmututallyrecursiveunaryprocedures,forexample:
letrec
even(x) = if zero?(x) then 1 else (odd -(x,1))
odd(x) = if zero?(x) then 0 else (even -(x,1))
in (odd 13)

3.4 LETREC:ALanguagewithRecursiveProcedures 85
= (-
(apply-procedure
(value-of
<<double>>
[x=(cid:13)6(cid:14)](extend-env-rec
double x <<if zero?(x) ...>> ρ ))
0
(value-of
<<-(x,1)>>
[x=(cid:13)6(cid:14)](extend-env-rec
double x <<if zero?(x) ...>> ρ )))
0
-2)
= (-
(apply-procedure
(procedure x <<if zero?(x) ...>>
(extend-env-rec double x <<if zero?(x) ...>> ρ ))
0
(cid:13)5(cid:14))
-2)
= ...
Figure3.11 Acalculationwithextend-env-rec,cont’d.
(cid:3)(cid:3)
Exercise3.33 [ ]Extendthelanguageabovetoallowthedeclarationofanynum-
berofmutuallyrecursiveprocedures,eachofpossiblymanyarguments,asinexer-
cise3.21.
(cid:3)(cid:3)(cid:3)
Exercise3.34 [ ]Implementextend-env-recintheproceduralrepresentation
ofenvironmentsfromsection2.2.3.
(cid:3)
Exercise3.35 [ ]Therepresentationswehaveseensofarareinefficient,becausethey
buildanewclosureeverytimetheprocedureisretrieved.Buttheclosureisthesame
everytime. Wecanbuildtheclosuresonlyonce,byputtingthevalueinavectorof
length1andbuildinganexplicitcircularstructure,like

86 3 Expressions
(define-datatype environment environment?
(empty-env)
(extend-env
(var identifier?)
(val expval?)
(env environment?))
(extend-env-rec
(p-name identifier?)
(b-var identifier?)
(body expression?)
(env environment?)))
(define apply-env
(lambda (env search-var)
(cases environment env
(empty-env ()
(report-no-binding-found search-var))
(extend-env (saved-var saved-val saved-env)
(if (eqv? saved-var search-var)
saved-val
(apply-env saved-env search-var)))
(extend-env-rec (p-name b-var p-body saved-env)
(if (eqv? search-var p-name)
(proc-val (procedure b-var p-body env))
(apply-env saved-env search-var))))))
Figure3.12 extend-env-recaddedtoenvironments.
Here’sthecodetobuildthisdatastructure.
(define extend-env-rec
(lambda (p-name b-var body saved-env)
(let ((vec (make-vector 1)))
(let ((new-env (extend-env p-name vec saved-env)))
(vector-set! vec 0
(proc-val (procedure b-var body new-env)))
new-env))))
Completetheimplementationofthisrepresentationbymodifyingthedefinitionsof
the environment data type and apply-env accordingly. Be sure that apply-env
alwaysreturnsanexpressedvalue.

| 3.5 ScopingandBindingofVariables |     |     |     |     | 87  |
| -------------------------------- | --- | --- | --- | --- | --- |
(cid:3)(cid:3)
Exercise3.36 [ ]Extendthisimplementationtohandlethelanguagefromexercise
3.32.
(cid:3)
Exercise3.37 [ ]Withdynamicbinding(exercise3.28),recursiveproceduresmaybe
bound by let; no special mechanism is necessaryfor recursion. This is of histori-
calinterest;intheearlyyearsofprogramminglanguagedesignotherapproachesto
recursion, such as those discussed in section 3.4, were not widely understood. To
demonstraterecursionviadynamicbinding,testtheprogram
| let    | fact = proc | (n) add1(n) |     |     |     |
| ------ | ----------- | ----------- | --- | --- | --- |
| in let | fact = proc | (n)         |     |     |     |
if zero?(n)
then 1
|     |     | else *(n,(fact | -(n,1))) |     |     |
| --- | --- | -------------- | -------- | --- | --- |
in (fact 5)
usingbothlexicaland dynamicbinding. Writethemutuallyrecursiveprocedures
evenandoddasinsection
3.4inthedefinedlanguagewithdynamicbinding.
| 3.5 Scopingand | Binding | of Variables |     |     |     |
| -------------- | ------- | ------------ | --- | --- | --- |
Wehavenowseenavarietyofsituationsinwhichvariablesaredeclaredand
used. Wenowdiscusstheseideasinamoresystematicway.
In most programming languages, variables may appear in two different
ways: asreferencesorasdeclarations. Avariablereferenceisauseofthevari-
able. Forexample,intheSchemeexpression
(f x y)
allthevariables,f,x,andy,appearasreferences.However,in
(lambda (x) (+ x 3))
or
| (let ((x | (+ y 7))) | (+ x 3)) |     |     |     |
| -------- | --------- | -------- | --- | --- | --- |
thefirstoccurrenceofxisadeclaration: itintroducesthevariableasaname
lambdaexpression,
| forsome value. | Inthe |     | the valueof | the variablewill | be  |
| -------------- | ----- | --- | ----------- | ---------------- | --- |
supplied when the procedureis called. In the let expression, the value of
| thevariableisobtainedfromthevalueoftheexpression(+ |     |     |     | y 7). |     |
| -------------------------------------------------- | --- | --- | --- | ----- | --- |
Wesaythatavariablereferenceisboundbythedeclarationwithwhichitis
associated, andthatitisbound toits value. We havealreadyseenexamples
ofavariablebeingboundbyadeclaration,insection1.2.4.

88 3 Expressions
|     | Figure3.13 | Asimplecontourdiagram |
| --- | ---------- | --------------------- |
Declarationsinmostprogramminglanguageshavealimitedscope,sothat
thesamevariablenamemaybeusedfordifferentpurposesindifferentparts
of a program. For example, we have repeatedlyused lst asa bound vari-
able,andineachcaseitsscopewaslimitedtothebodyofthecorresponding
lambdaexpression.
Everyprogramminglanguagemusthavesomerulestodeterminethedec-
laration to which each variable reference refers. These rules are typically
called scoping rules. The portion of the program in which a declaration is
validiscalledthescopeofthedeclaration.
We can determine which declarationis associated with eachvariableuse
withoutexecutingtheprogram. Propertieslikethis,whichcanbecomputed
withoutexecutingtheprogram,arecalledstaticproperties.
To find which declaration corresponds to a given use of a variable, we
searchoutwardfromtheuseuntilwefindadeclarationofthevariable. Here
isasimpleexampleinScheme.
| (let ((x | 3)        | Callthisx1      |
| -------- | --------- | --------------- |
|          | (y 4))    |                 |
| (+ (let  | ((x       | Callthisx2      |
|          | (+ y 5))) |                 |
|          | (* x y))  | Herexreferstox2 |
| x))      |           | Herexreferstox1 |

3.5 ScopingandBindingofVariables 89
Inthisexample,theinnerxisboundto9,sothevalueoftheexpressionis
(let ((x 3)
(y 4))
(+ (let ((x
(+ y 5)))
(* x y))
x))
= (+ (let ((x
(+ 4 5)))
(* x 4))
3)
= (+ (let ((x 9))
(* x 4))
3)
= (+ 36
3)
= 39
Scoping rules like this are called lexical scoping rules, and the variables
declaredinthiswayarecalledlexicalvariables.
Under lexical scoping, we can create a hole in a scope by redeclaring a
variable. Such an inner declaration shadows the outer one. For instance, in
theexampleabove,theinnerxshadowstheouterone inthemultiplication
(* x y).
Lexical scopes are nested: each scope lies entirely within another scope.
Wecanillustratethiswithacontourdiagram. Figure3.13showsthecontour
diagramfortheexampleabove. Aboxsurroundseachscope,andavertical
lineconnectseachdeclarationtoitsscope.
Figure3.14showsamorecomplicatedprogramwiththe contoursdrawn
in. Heretherearethreeoccurrencesof theexpression(+ x y z), on lines
5,7,and8.Line5iswithinthescopeofx2andz2,whichiswithinthescope
ofz1,whichiswithinthescopeofx1andy1. Soatline5,xreferstox2,y
referstoy1,andz referstoz2.Line7iswithinthescopeofx4andy2,which
iswithin the scope of x2 andz2, which iswithin the scope of z1, which is
withinthescopeofx1andy1. Soatline7,xreferstox4,yreferstoy2,and
zreferstoz2. Last,line8iswithinthescopeofx3,whichiswithinthescope
ofx2andz2, whichiswithin thescopeof z1, whichiswithin thescopeof
x1andy1. Soatline8,xreferstox3,yreferstoy1,andzreferstoz2.

90 3 Expressions
Figure3.14
Amorecomplicatedcontourdiagram
Theassociationbetweenavariableanditsvalueiscalledabinding.Forour
language,wecanlookatthespecificationtoseehowthebindingiscreated.
Avariabledeclaredbyaprocisboundwhentheprocedureisapplied.
| (apply-procedure |     | (procedure | var body | ρ) val) |
| ---------------- | --- | ---------- | -------- | ------- |
ρ))
| = (value-of | body | (extend-env | var val |     |
| ----------- | ---- | ----------- | ------- | --- |
Alet-variableisboundbythevalueofitsright-handside.
ρ)
| (value-of   | (let-exp | var val     | body)   |     |
| ----------- | -------- | ----------- | ------- | --- |
| = (value-of | body     | (extend-env | var val | ρ)) |
A variable declared by a letrec is bound using its right-hand side as
well.
(value-of
| (letrec-exp |     |                     |           | letrec-body) |
| ----------- | --- | ------------------- | --------- | ------------ |
|             |     | proc-name bound-var | proc-body |              |
ρ)
= (value-of
letrec-body
ρ))
| (extend-env-rec |     | proc-name | bound-var | proc-body |
| --------------- | --- | --------- | --------- | --------- |
The extent of a binding is the time interval during which the binding is
maintained. In our little language, as in Scheme, all bindings have semi-
infinite extent, meaning that once a variable gets bound, that binding must
be maintained indefinitely (at least potentially). This is because the bind-
ing might be hidden inside a closure that is returned. In languages with
semi-infiniteextent,thegarbagecollectorcollectsbindingswhentheyareno
longer reachable. This isonly determinableatrun-time, sowe saythatthis
isadynamicproperty.

3.6 EliminatingVariableNames 91
Regrettably, “dynamic” is sometimes used to mean “during the evalua-
tion of an expression” but other times is used to mean “not calculable in
advance.” Ifwedidnotallowaproceduretobeusedasthevalueofalet,
then the let-bindings would expire at the end of the evaluation of the let
body. This is called dynamic extent, and it is a static property. Because the
extentisastaticproperty,wecanpredictexactlywhenabindingcanbedis-
carded.Dynamicbinding,asinexercise3.28etseq.,behavessimilarly.
3.6 EliminatingVariableNames
Execution of the scoping algorithm may then be viewed as a journey out-
wardfromavariablereference.Inthisjourneyanumberofcontoursmaybe
crossedbeforearrivingattheassociateddeclaration.Thenumberofcontours
crossediscalledthelexical(orstatic)depthofthevariablereference. Itiscus-
tomarytouse “zero-basedindexing,” therebynotcounting thelastcontour
crossed. Forexample,intheSchemeexpression
(lambda (x)
((lambda (a)
(x a))
x))
the reference to x on the last line and the reference to a have lexical depth
zero,whilethereferencetoxinthethirdlinehaslexicaldepthone.
We could, therefore, get rid of variable names entirely, and write some-
thinglike
(nameless-lambda
((nameless-lambda
(#1 #0))
#0))
Here each nameless-lambda declares a new anonymous variable, and
eachvariablereferenceisreplacedbyitslexicaldepth;thisnumberuniquely
identifiesthedeclarationtowhichitrefers. Thesenumbersarecalledlexical
addressesordeBruijnindices.Compilersroutinelycalculatethelexicaladdress
ofeachvariablereference.Oncethishasbeendone,thevariablenamesmay
bediscardedunlesstheyarerequiredtoprovidedebugginginformation.
Thiswayofrecordingtheinformationisusefulbecausethelexicaladdress
predictsjustwhereintheenvironmentanyparticularvariablewillbefound.

92 3 Expressions
Considertheexpression
let x = exp
1
in let y = exp
2
in -(x,y)
in our language. In the differenceexpression, the lexicaldepths of y and x
are0and1,respectively.
Nowassumethatthevaluesofexp andexp ,intheappropriateenviron-
1 2
ments,areval andval . Thenthevalueofthisexpressionis
1 2
(value-of
<<let x = exp
1
in let y = exp
2
in -(x,y)>>
ρ)
=
(value-of
<<let y = exp
2
in -(x,y)>>
[x=val ]ρ)
1
=
(value-of
<<-(x,y)>>
[y=val ][x=val ]ρ)
2 1
sothatwhenthedifferenceexpressionisevaluated,yisatdepth0andxis
atdepth1,justaspredictedbytheirlexicaldepths.
Ifweareusinganassociation-listrepresentationofenvironments(seeexer-
cise2.5),thentheenvironmentwilllooklike
saved−env
y val2 x val1
sothatthevaluesofxandywillbefoundbytakingeither1cdror0cdrsin
theenvironment,regardlessofthevaluesval andval .
1 2
Thesamethingworksforprocedurebodies. Consider
let a = 5
in proc (x) -(x,a)
Inthebodyoftheprocedure,xisatlexicaldepth0andaisatdepth1.

3.7 ImplementingLexicalAddressing 93
Thevalueofthisexpressionis
(value-of
| <<let a | = 5 in proc | (x) -(x,a)>> |
| ------- | ----------- | ------------ |
ρ)
| = (value-of | <<proc            | (x) -(x,a)>> |
| ----------- | ----------------- | ------------ |
|             | (cid:13)5(cid:14) | ρ))          |
| (extend-env | a                 |              |
[a=(cid:13)5(cid:14)]ρ))
| = (proc-val | (procedure | x <<-(x,a)>> |
| ----------- | ---------- | ------------ |
Thebodyofthisprocedurecanonlybeevaluatedbyapply-procedure:
(apply-procedure
| (procedure | x <<-(x,a)>> | [a=(cid:13)5(cid:14)]ρ) |
| ---------- | ------------ | ----------------------- |
(cid:13)7(cid:14))
| = (value-of | <<-(x,a)>> |     |
| ----------- | ---------- | --- |
[x=(cid:13)7(cid:14)][a=(cid:13)5(cid:14)]ρ)
Soagaineveryvariableisfoundintheenvironmentattheplacepredicted
byitslexicaldepth.
3.7 ImplementingLexicalAddressing
We now implement the lexical-address analysis we sketched above. We
write aprocedure translation-of-program thattakesa programand
removesallthevariablesfromthedeclarations,andreplaceseveryvariable
referencebyitslexicaldepth.
Forexample,theprogram
| let x   | = 37 |     |
| ------- | ---- | --- |
| in proc | (y)  |     |
let z = -(y,x)
in -(x,y)
istranslatedto
#(struct:a-program
#(struct:nameless-let-exp
| #(struct:const-exp |     | 37) |
| ------------------ | --- | --- |
#(struct:nameless-proc-exp
#(struct:nameless-let-exp
#(struct:diff-exp
#(struct:nameless-var-exp 0)
#(struct:nameless-var-exp 1))
#(struct:diff-exp
#(struct:nameless-var-exp 2)
#(struct:nameless-var-exp 1))))))
value-of-program
Wethenwriteanewversionof thatwillfindthevalue
ofsuchanamelessprogram,withoutputtingvariablesintheenvironment.

94 3 Expressions
3.7.1 TheTranslator
We are writing a translator, so we need to know the source language
and the target language. The target language will have things like
nameless-var-expandnameless-let-expthatwerenotinthesource
language, and it will lose the things in the source language that these con-
structsreplace,likevar-expandlet-exp.
Wecaneitherwriteoutdefine-datatype’sforeachlanguage,orwecan
set up a single define-datatype that includes both. Since we are using
SLLGENasourfrontend,itiseasiertodothelatter.WeaddtotheSLLGEN
grammartheproductions
Expression::=%lexref number
nameless-var-exp (num)
Expression::=%let Expression in Expression
nameless-let-exp (exp1 body)
Expression::=%lexproc Expression
nameless-proc-exp (body)
We use names starting with % for these new constructs because % is nor-
mallythecommentcharacterinourlanguage.
Our translator will reject any program that has one of these new name-
lessconstructs(nameless-var-exp,nameless-let-exp,ornameless-
proc-exp),and our interpreterwill rejectanyprogramthathasone of the
old nameful constructs (var-exp, let-exp, or proc-exp) that are sup-
posedtobereplaced.
Tocalculatethelexicaladdressofanyvariablereference,weneedtoknow
thescopesinwhichitisenclosed. Thisiscontextinformation,soitshouldbe
liketheinheritedattributesinsection1.3.
Sotranslation-ofwilltaketwoarguments: anexpressionandastatic
environment. The static environment will be a list of variables, representing
thescopeswithinwhichthecurrentexpressionlies. Thevariabledeclaredin
theinnermostscopewillbethefirstelementofthelist.
For example, when we translate the last line of the example above, the
staticenvironmentshouldbe
(z y x)
Solookingupavariableinthestaticenvironmentmeansfindingitsposition
in the static environment, which gives a lexical address: looking up x will
give2,lookingupywillgive1,andlookingupzwillgive0.

3.7 ImplementingLexicalAddressing 95
Senv = Listof(Sym)
| Lexaddr    | = N         |     |     |
| ---------- | ----------- | --- | --- |
| empty-senv | : () → Senv |     |     |
| (define    | empty-senv  |     |     |
| (lambda    | ()          |     |     |
’()))
|             | ×           | →       |     |
| ----------- | ----------- | ------- | --- |
| extend-senv | : Var Senv  | Senv    |     |
| (define     | extend-senv |         |     |
| (lambda     | (var senv)  |         |     |
| (cons       | var senv))) |         |     |
|             | ×           | →       |     |
| apply-senv  | : Senv Var  | Lexaddr |     |
| (define     | apply-senv  |         |     |
| (lambda     | (senv var)  |         |     |
(cond
|     | ((null? senv)       |        |     |
| --- | ------------------- | ------ | --- |
|     | (report-unbound-var | var))  |     |
|     | ((eqv? var (car     | senv)) |     |
0)
(else
|     | (+ 1 (apply-senv                              | (cdr senv) | var)))))) |
| --- | --------------------------------------------- | ---------- | --------- |
|     | Figure3.15 Implementationofstaticenvironments |            |           |
Enteringanewscopewill meanaddinga newelementtothe staticenvi-
ronment. Weintroduceaprocedureextend-senvtodothis.
Sincethestaticenvironmentisjustalistofvariables,theseproceduresare
easytoimplementandareshowninfigure3.15.
For the translator, we have two procedures, translation-of, which
handlesexpressions,andtranslation-of-program,whichhandlespro-
grams.
Wearetryingtotranslateanexpressionewhichissittinginsidethedecla-
rationsrepresentedbysenv. Todothis,werecursivelycopythetree,aswe
didinexercises1.33or2.26,exceptthat
1. Everyvar-expisreplacedbyanameless-var-expwiththerightlex-
icaladdress,whichwecomputebycallingapply-senv.

96 3 Expressions
2. Every let-exp is replaced by a nameless-let-exp. The right-hand
side of the new expression will be the translation of the right-hand side
of the old expression. This is in the same scope as the original, so we
translate it in the same static environment senv. The body of the new
expression will be the translation of the body of the old expression. But
thebodynowliesinanewscope,withtheadditionalboundvariablevar.
So we translate the body in the static environment (extend-senv var
senv).
| 3. Everyproc-expisreplacedbyanameless-proc-exp,withthe |     |     | body |
| ------------------------------------------------------ | --- | --- | ---- |
translated with respect to the new scope, represented by the static envi-
| ronment(extend-senv | var senv). |     |     |
| ------------------- | ---------- | --- | --- |
Thecodefortranslation-ofisshowninfigure3.16.
| Theproceduretranslation-of-program |     | runstranslation-of | in  |
| ---------------------------------- | --- | ------------------ | --- |
asuitableinitialstaticenvironment.
→
| translation-of-program | : Program | Nameless-program |     |
| ---------------------- | --------- | ---------------- | --- |
(define translation-of-program
(lambda (pgm)
| (cases program | pgm    |     |     |
| -------------- | ------ | --- | --- |
| (a-program     | (exp1) |     |     |
(a-program
(translation-of exp1 (init-senv)))))))
| init-senv : () → | Senv |     |     |
| ---------------- | ---- | --- | --- |
(define init-senv
(lambda ()
| (extend-senv | ’i  |     |     |
| ------------ | --- | --- | --- |
| (extend-senv | ’v  |     |     |
(extend-senv ’x
(empty-senv))))))
3.7.2 TheNamelessInterpreter
Ourinterpretertakesadvantageofthepredictionsofthelexical-addressana-
lyzertoavoidexplicitlysearchingforvariablesatruntime.
Sincetherearenomorevariablesinourprograms,wewon’tbeabletoput
variablesin our environments, but since we know exactlywhere to look in
eachenvironment,wedon’tneedthem!

3.7 ImplementingLexicalAddressing 97
| translation-of | : Exp × Senv   | → Nameless-exp |       |
| -------------- | -------------- | -------------- | ----- |
| (define        | translation-of |                |       |
| (lambda        | (exp senv)     |                |       |
| (cases         | expression     | exp            |       |
| (const-exp     | (num)          | (const-exp     | num)) |
| (diff-exp      | (exp1          | exp2)          |       |
(diff-exp
|            | (translation-of | exp1 | senv)   |
| ---------- | --------------- | ---- | ------- |
|            | (translation-of | exp2 | senv))) |
| (zero?-exp | (exp1)          |      |         |
(zero?-exp
|         | (translation-of | exp1  | senv))) |
| ------- | --------------- | ----- | ------- |
| (if-exp | (exp1 exp2      | exp3) |         |
(if-exp
|          | (translation-of | exp1 | senv)   |
| -------- | --------------- | ---- | ------- |
|          | (translation-of | exp2 | senv)   |
|          | (translation-of | exp3 | senv))) |
| (var-exp | (var)           |      |         |
(nameless-var-exp
|          | (apply-senv | senv var))) |     |
| -------- | ----------- | ----------- | --- |
| (let-exp | (var exp1   | body)       |     |
(nameless-let-exp
|           | (translation-of | exp1         | senv) |
| --------- | --------------- | ------------ | ----- |
|           | (translation-of | body         |       |
|           | (extend-senv    | var senv)))) |       |
| (proc-exp | (var            | body)        |       |
(nameless-proc-exp
|           | (translation-of | body         |     |
| --------- | --------------- | ------------ | --- |
|           | (extend-senv    | var senv)))) |     |
| (call-exp | (rator          | rand)        |     |
(call-exp
|     | (translation-of | rator | senv)   |
| --- | --------------- | ----- | ------- |
|     | (translation-of | rand  | senv))) |
(else
(report-invalid-source-expression exp)))))
|     | Figure3.16 | Thelexical-addresstranslator |     |
| --- | ---------- | ---------------------------- | --- |

98 3 Expressions
Ourtop-levelprocedurewillberun:
→
| run : String | ExpVal   |     |     |     |
| ------------ | -------- | --- | --- | --- |
| (define      | run      |     |     |     |
| (lambda      | (string) |     |     |     |
(value-of-program
(translation-of-program
|     | (scan&parse | string))))) |     |     |
| --- | ----------- | ----------- | --- | --- |
Insteadofhavingfull-fledgedenvironments,wewillhavenamelessenvi-
ronments,withthefollowinginterface:
→
| nameless-environment? |     | : SchemeVal         | Bool         |              |
| --------------------- | --- | ------------------- | ------------ | ------------ |
| empty-nameless-env    |     | : () → Nameless-env |              |              |
|                       |     |                     | ×            | →            |
| extend-nameless-env   |     | : Expval            | Nameless-env | Nameless-env |
| apply-nameless-env    |     | : Nameless-env      | × Lexaddr    | → DenVal     |
Wecanimplementanamelessenvironmentasalistofdenotedvalues,so
| thatapply-nameless-envissimply |     |     | acalltolist-ref. |     |
| ------------------------------ | --- | --- | ---------------- | --- |
The implemen-
tationisshowninfigure3.17.
Atthelastlineoftheexampleonpage93,thenamelessenvironmentwill
looklike
saved−env
|     | value of z | value of y | value of x |     |
| --- | ---------- | ---------- | ---------- | --- |
Havingchangedtheenvironmentinterface,weneedtolookatallthecode
thatdependsonthatinterface. Thereareonly twothings inourinterpreter
proceduresandvalue-of.
thatuseenvironments:
The revised specification for proceduresis just the old one with the vari-
ablenameremoved.
ρ)
| (apply-procedure |      | (procedure           | body | val) |
| ---------------- | ---- | -------------------- | ---- | ---- |
| = (value-of      |      | (extend-nameless-env |      | ρ))  |
|                  | body |                      |      | val  |
Wecanimplementthisbydefining
| procedure        | : Nameless-exp | × Nameless-env | → Proc |     |
| ---------------- | -------------- | -------------- | ------ | --- |
| (define-datatype | proc           | proc?          |        |     |
(procedure
| (body               | expression?) |                          |     |     |
| ------------------- | ------------ | ------------------------ | --- | --- |
| (saved-nameless-env |              | nameless-environment?))) |     |     |

3.7 ImplementingLexicalAddressing 99
nameless-environment? : SchemeVal → Bool
(define nameless-environment?
(lambda (x)
((list-of expval?) x)))
empty-nameless-env : () → Nameless-env
(define empty-nameless-env
(lambda ()
’()))
extend-nameless-env : ExpVal × Nameless-env → Nameless-env
(define extend-nameless-env
(lambda (val nameless-env)
(cons val nameless-env)))
apply-nameless-env : Nameless-env × Lexaddr → ExpVal
(define apply-nameless-env
(lambda (nameless-env n)
(list-ref nameless-env n)))
Figure3.17 Namelessenvironments
apply-procedure : Proc × ExpVal → ExpVal
(define apply-procedure
(lambda (proc1 val)
(cases proc proc1
(procedure (body saved-nameless-env)
(value-of body
(extend-nameless-env val saved-nameless-env))))))
Now we can write value-of. Most cases are the same as in the earlier
interpreters except that where we used env we now use nameless-env.
We do have new cases, however, that correspond to var-exp, let-exp,
and proc-exp, which we replace by cases for nameless-var-exp,
nameless-let-exp,andnameless-proc-exp,respectively. Theimple-
mentation is shown in figure 3.18. A nameless-var-expgets looked up
in the environment. A nameless-let-exp evaluates its right-hand side
exp ,andthenevalutesitsbodyinanenvironmentextendedbythevalueof
1
theright-handside. Thisisjustwhatanordinaryletdoes,butwithoutthe
variables. A nameless-proc produces a proc, which is then applied by
apply-procedure.

100 3 Expressions
|          |                     |                      | ×            |                       | →                    |     |
| -------- | ------------------- | -------------------- | ------------ | --------------------- | -------------------- | --- |
| value-of | : Nameless-exp      |                      | Nameless-env |                       | ExpVal               |     |
| (define  | value-of            |                      |              |                       |                      |     |
| (lambda  | (exp                | nameless-env)        |              |                       |                      |     |
|          | (cases              | expression           | exp          |                       |                      |     |
|          | (const-exp          |                      | (num)        | ...asbefore...)       |                      |     |
|          | (diff-exp           |                      | (exp1 exp2)  | ...asbefore...)       |                      |     |
|          | (zero?-exp          |                      | (exp1)       | ...asbefore...)       |                      |     |
|          | (if-exp             | (exp1                | exp2         | exp3) ...asbefore...) |                      |     |
|          | (call-exp           |                      | (rator rand) | ...asbefore...)       |                      |     |
|          | (nameless-var-exp   |                      |              | (n)                   |                      |     |
|          | (apply-nameless-env |                      |              | nameless-env          |                      | n)) |
|          | (nameless-let-exp   |                      |              | (exp1 body)           |                      |     |
|          | (let                | ((val                | (value-of    | exp1                  | nameless-env)))      |     |
|          | (value-of           |                      | body         |                       |                      |     |
|          |                     | (extend-nameless-env |              |                       | val nameless-env)))) |     |
|          | (nameless-proc-exp  |                      |              | (body)                |                      |     |
(proc-val
|     | (procedure |     | body | nameless-env))) |     |     |
| --- | ---------- | --- | ---- | --------------- | --- | --- |
(else
|     | (report-invalid-translated-expression |     |                                   |     |     | exp))))) |
| --- | ------------------------------------- | --- | --------------------------------- | --- | --- | -------- |
|     | Figure3.18                            |     | value-offorthenamelessinterpreter |     |     |          |
Last,here’sthenewvalue-of-program:
| value-of-program |                  | :       | Nameless-program              | →   | ExpVal |     |
| ---------------- | ---------------- | ------- | ----------------------------- | --- | ------ | --- |
| (define          | value-of-program |         |                               |     |        |     |
| (lambda          | (pgm)            |         |                               |     |        |     |
|                  | (cases           | program | pgm                           |     |        |     |
|                  | (a-program       |         | (exp1)                        |     |        |     |
|                  | (value-of        |         | exp1 (init-nameless-env)))))) |     |        |     |

3.7 ImplementingLexicalAddressing 101
(cid:3)
Exercise3.38 [ ]Extendthelexicaladdresstranslatorandinterpretertohandlecond
fromexercise3.12.
(cid:3)
Exercise3.39 [ ]Extendthelexicaladdresstranslatorandinterpretertohandlepack
andunpackfromexercise3.18.
(cid:3)(cid:3)
Exercise3.40 [ ]Extend the lexical address translator and interpreter to handle
letrec. Do this by modifyingthe contextargumentto translation-ofso that
itkeepstrackofnotonlythenameofeachboundvariable,butalsowhetheritwas
boundbyletrecornot. Forareferencetoavariablethatwasboundbyaletrec,
generateanewkindofreference,calleda nameless-letrec-var-exp. Youcan
thencontinuetousethenamelessenvironmentrepresentationabove,andtheinter-
pretercandotherightthingwithanameless-letrec-var-exp.
(cid:3)(cid:3)
Exercise3.41 [ ]Modifythelexicaladdresstranslatorandinterpretertohandlelet
expressions, procedures, and procedure calls with multiple arguments, as in exer-
cise3.21. Dothisusinganamelessversionoftheribcagerepresentationofenviron-
ments(exercise2.11). Forthisrepresentation,thelexicaladdresswillconsistoftwo
nonnegativeintegers: thelexicaldepth,toindicatethenumberofcontourscrossed,
asbefore;andaposition,toindicatethepositionofthevariableinthedeclaration.
(cid:3)(cid:3)(cid:3)
Exercise3.42 [ ]Modifythelexicaladdresstranslatorandinterpretertousethe
trimmedrepresentationofproceduresfromexercise3.26. Forthis,youwillneedto
translate the body of the procedure not (extend-senv var senv), but in a new
staticenvironmentthattellsexactlywhereeachvariablewillbekeptinthetrimmed
representation.
(cid:3)(cid:3)(cid:3)
Exercise3.43 [ ]Thetranslatorcandomorethanjustkeeptrackofthenamesof
variables.Forexample,considertheprogram
let x = 3
in let f = proc (y) -(y,x)
in (f 13)
Herewecantellstaticallythatattheprocedurecall,fwillbeboundtoaprocedure
whose body is -(y,x), where x has the same value that it had at the procedure-
creation site. Therefore we could avoid looking up f in the environment entirely.
Extend the translator to keep track of “known procedures”and generate code that
avoidsanenvironmentlookupatthecallofsuchaprocedure.
(cid:3)(cid:3)(cid:3)
Exercise3.44 [ ]Intheprecedingexample,theonlyuseoffisasaknownpro-
cedure.Thereforetheprocedurebuiltbytheexpressionproc (y) -(y,x)isnever
used.Modifythetranslatorsothatsuchaprocedureisneverconstructed.

4
State
4.1 ComputationalEffects
Sofar,wehaveonlyconsideredthevalueproducedbyacomputation. Buta
computationmayhaveeffectsaswell: itmayread,print,oralterthestateof
memoryorafilesystem. Intherealworld,wearealwaysinterestedineffects:
ifacomputationdoesn’tdisplayitsanswer,itdoesn’tdousanygood!
What’sthedifferencebetweenproducingavalueandproducinganeffect?
Aneffectisglobal: itisseenbytheentirecomputation. Aneffectaffectsthe
entirecomputation(punintended).
Wewillbeconcernedprimarilywithasingleeffect: assignmenttoaloca-
tion in memory. How does assignment differ from binding? As we have
seen, binding is local, but variable assignment is potentially global. It is
aboutthesharingofvaluesbetweenotherwiseunrelatedportionsofthecom-
putation. Twoprocedurescanshareinformationiftheybothknowaboutthe
samelocation inmemory. A single procedurecanshareinformationwith a
futureinvocationofitselfbyleavingtheinformationinaknownlocation.
We model memory as a finite map fromlocationsto a setof valuescalled
the storablevalues. For historicalreasons, we callthis the store. The storable
valuesinalanguagearetypically,butnotalways,thesameastheexpressed
valuesofthelanguage. Thischoiceispartofthedesignofalanguage.
Adatastructurethatrepresentsalocation iscalledareference. A location
isaplace inmemory whereavalue canbe stored, and areferenceisadata
structurethatreferstothatplace. Thedistinctionbetweenlocationsandref-
erencesmaybeseenbyanalogy: alocationislikeafileandareferenceislike
aURL.TheURLreferstothefile,andthefilecontainssomedata. Similarly,
areferencedenotesalocation,andthelocationcontainssomedata.

104 4 State
Referencesare sometimes called L-values. This name reflects the associa-
tion of such data structures with variables appearing on the left-hand side
ofassignmentstatements. Analogously,expressedvalues,suchasthevalues
of the right-hand side expressions of assignment statements, are known as
R-values.
Weconsidertwodesignsforalanguagewithastore.Wecallthesedesigns
explicitreferencesandimplicitreferences.
4.2 EXPLICIT-REFS:A Languagewith ExplicitReferences
In this design, we add referencesas a new kind of expressed value. So we
have
= + + +
ExpVal Int Bool Proc Ref(ExpVal)
=
DenVal ExpVal
Here Ref(ExpVal) means the set of references to locations that contain
expressedvalues.
We leave the binding structures of the language unchanged, but we add
threenewoperationstocreateandusereferences.
• newref,whichallocatesanewlocationandreturnsareferencetoit.
• deref, which dereferencesa reference: that is, it returnsthe contents of
thelocationthatthereferencerepresents.
• setref,whichchangesthecontentsofthelocationthatthereferencerep-
resents.
WecalltheresultinglanguageEXPLICIT-REFS.Let’swritesomeprograms
usingtheseconstructs.
Below are two procedures, even and odd. They each take an argument,
which they ignore, and return 1 or 0 depending on whether the contents
of the location x is even or odd. They communicate not by passing data
explicitly,butbychangingthecontentsofthevariabletheyshare.
Thisprogramdetermineswhetheror not13isodd,andthereforereturns
1. The procedures even and odd do not refer to their arguments; instead
theylookatthecontentsofthelocationtowhichxisbound.

| 4.2 EXPLICIT-REFS:ALanguagewithExplicitReferences |             |     |     | 105 |
| ------------------------------------------------- | ----------- | --- | --- | --- |
| let x = newref(0)                                 |             |     |     |     |
| in letrec                                         | even(dummy) |     |     |     |
= if zero?(deref(x))
then 1
else begin
|     |     | setref(x, -(deref(x),1)); |     |     |
| --- | --- | ------------------------- | --- | --- |
|     |     | (odd 888)                 |     |     |
end
odd(dummy)
= if zero?(deref(x))
then 0
else begin
|     |     | setref(x, -(deref(x),1)); |     |     |
| --- | --- | ------------------------- | --- | --- |
|     |     | (even 888)                |     |     |
end
| in begin | setref(x,13); | (odd 888) | end |     |
| -------- | ------------- | --------- | --- | --- |
This program uses multideclaration letrec (exercise 3.32) and a begin
expression (exercise 4.4). A begin expression evaluates its subexpressions
inorderandreturnsthevalueofthelastone.
We pass a dummy argument to even and odd to stay within the frame-
work of our unarylanguage; if we had proceduresof any number of argu-
ments(exercise3.21)wecouldhavemadetheseproceduresofnoarguments.
This style of communication is convenient when two procedures might
share many quantities; one needs to assign only to the few quantities that
changefromonecalltothenext. Similarly,oneproceduremightcallanother
procedure not directly but through a long chain of procedure calls. They
could communicate data directly through a shared variable, without the
intermediate procedures needing to know about it. Thus communication
throughasharedvariablecanbeakindofinformationhiding.
Another use of assignment is to create hidden state through the use of
privatevariables.Hereisanexample.
| let g = let | counter | = newref(0) |     |     |
| ----------- | ------- | ----------- | --- | --- |
in proc (dummy)
begin
|     | setref(counter, | -(deref(counter), |     | -1)); |
| --- | --------------- | ----------------- | --- | ----- |
deref(counter)
end
| in let a | = (g 11)   |     |     |     |
| -------- | ---------- | --- | --- | --- |
| in let   | b = (g 11) |     |     |     |
| in       | -(a,b)     |     |     |     |
Here the procedure g keeps a private variable that stores the number of
timesghasbeencalled. Hencethefirstcalltogreturns1,thesecondcallto
greturns2,andtheentireprogramhasthevalue-1.

106 4 State
Hereisapictureoftheenvironmentinwhichgisbound.
We can think of this as the different invocations of g sharing information
witheachother. ThistechniqueisusedbytheSchemeproceduregensymto
createuniquesymbols.
(cid:3)
Exercise4.1 [ ]Whatwouldhavehappenedhadtheprogrambeeninstead
let g = proc (dummy)
let counter = newref(0)
in begin
setref(counter, -(deref(counter), -1));
deref(counter)
end
in let a = (g 11)
in let b = (g 11)
in -(a,b)
InEXPLICIT-REFS, we canstore anyexpressedvalue, and referencesare
expressedvalues.Thismeanswecanstoreareferenceinalocation. Consider
theprogram
let x = newref(newref(0))
in begin
setref(deref(x), 11);
deref(deref(x))
end

| 4.2 EXPLICIT-REFS:ALanguagewithExplicitReferences |     |     |     |     |     |     | 107 |
| ------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
This program allocates a new location containing 0. It then binds x to
a location containing a reference to the first location. Hence the value of
deref(x)isareferencetothefirstlocation. Sowhentheprogramevaluates
the setref, it is the first location that is modified, and the entire program
returns11.
4.2.1 Store-PassingSpecifications
Inourlanguage,anyexpressionmayhaveaneffect. Tospecifytheseeffects,
weneedtodescribewhatstoreshouldbeusedforeachevaluationandhow
eachevaluationcanmodifythestore.
|     |     | σ   |     |     |     |     | = σ |
| --- | --- | --- | --- | --- | --- | --- | --- |
Inour specifications, we use torange over stores. We write [l v] to
meanastorejustlike σ ,exceptthatlocationlismappedtov. Whenwerefer
σ
| toaparticularvalueof |     | ,wesometimescallitthestateofthestore. |     |     |     |     |     |
| -------------------- | --- | ------------------------------------- | --- | --- | --- | --- | --- |
Weusestore-passingspecfications. Inastore-passingspecification,thestore
ispassedasanexplicitargumenttovalue-ofandisreturnedasanexplicit
resultfromvalue-of.Thuswewrite
|     |           |     | ρ σ | )=(val | ,σ  |     |     |
| --- | --------- | --- | --- | ------ | --- | --- | --- |
|     | (value-of | exp |     |        |     | )   |     |
|     |           |     | 1 0 |        | 1   | 1   |     |
This asserts that expression exp , evaluated in environment ρ and with the
1
σ
store in state , returns the value val and leaves the store in a possibly
|     | 0   |     | 1   |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
σ
| differentstate | 1 . |     |     |     |     |     |     |
| -------------- | --- | --- | --- | --- | --- | --- | --- |
Thuswecanspecifyaneffect-freeoperationlikeconst-expbywriting
|     | (value-of | (const-exp |     | n) ρ | σ)=(n,σ) |     |     |
| --- | --------- | ---------- | --- | ---- | -------- | --- | --- |
showingthatthestoreisunchangedbyevaluationofthisexpression.
Thespecificationfordiff-expshowshowwespecifysequentialbehav-
ior.
|           | (value-of | exp | ρ σ   | )=(val                  | ,σ  | )                 |                    |
| --------- | --------- | --- | ----- | ----------------------- | --- | ----------------- | ------------------ |
|           |           |     | 1 0   |                         | 1   | 1                 |                    |
|           |           |     | ρ σ   | )=(val                  | ,σ  |                   |                    |
|           | (value-of | exp |       |                         |     | )                 |                    |
|           |           |     | 2 1   |                         | 2   | 2                 |                    |
|           |           |     |       | ρ σ )=((cid:13)(cid:16) |     | (cid:17)-(cid:16) | (cid:17)(cid:14),σ |
| (value-of | (diff-exp | exp | exp ) |                         |     | val val           | )                  |
|           |           |     | 1 2   | 0                       |     | 1                 | 2 2                |
Hereweevaluateexp startingwiththestoreinstate σ . exp returnsvalue
|     | 1   |     |     |     |     | 0 1 |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
σ
val , but itmight also have some effectsthatleave the store in state . We
| 1   |     |     |     |     |     |     | 1   |
| --- | --- | --- | --- | --- | --- | --- | --- |
thenevaluateexp startingwiththestoreinthestatethatexp leftit,namely
|     | 2   |     |     |     |     | 1   |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
| σ   |     |     |     |     |     |     | σ   |
. exp similarlyreturnsavalueval andleavesthestoreinstate . Then
| 1 2 |     |     | 2   |     |     |     | 2   |
| --- | --- | --- | --- | --- | --- | --- | --- |
−
the entire expression returns val 1 val 2 without further effecton the store,
σ
| soitleavesthestoreinstate |     | .   |     |     |     |     |     |
| ------------------------- | --- | --- | --- | --- | --- | --- | --- |
2

108 4 State
Let’stryaconditional.
(value-of exp ρ σ )=(val ,σ )
1 0 1 1
(val (cid:2) ue-of (if-exp exp 1 exp 2 exp 3 ) ρ σ 0 )
(value-of exp ρ σ ) if(expval->boolval ) = #t
= 2 1 1
(value-of exp ρ σ ) if(expval->boolval ) = #f
3 1 1
Startingin state σ , anif-exp evaluatesits test expression exp , return-
0 1
σ
ing the value val and leaving the store in state . The result of the entire
1 1
expression is then either the result of exp or exp , each evaluated in the
2 3
ρ σ
currentenvironment andinthestate inwhichexp leftthestore.
1 1
(cid:3)
Exercise4.2 [ ]Writedownthespecificationforazero?-exp.
(cid:3)
Exercise4.3 [ ]Writedownthespecificationforacall-exp.
(cid:3)(cid:3)
Exercise4.4 [ ]Writedownthespecificationforabeginexpresssion.
Expression::=begin Expression {; Expression}∗ end
A begin expression may contain one or more subexpressions separated by semi-
colons.Theseareevaluatedinorderandthevalueofthelastisreturned.
(cid:3)(cid:3)
Exercise4.5 [ ]Writedownthespecificationforlist(exercise3.10).
4.2.2 SpecifyingOperationsonExplicitReferences
In EXPLICIT-REFS, we have three new operations that must be specified:
newref,deref,andsetref.Thesearegivenbythegrammar
Expression::=newref (Expression)
newref-exp (exp1)
Expression::=deref (Expression)
deref-exp (exp1)
Expression::=setref (Expression , Expression)
setref-exp (exp1 exp2)
Wecanspecifythebehavioroftheseoperationsasfollows.
(value-of exp ρ σ )=(val,σ ) l (cid:3)∈ dom( σ )
0 1 1
(value-of (newref-exp exp) ρ σ )=((ref-val l),[l=val]σ )
0 1

| 4.2 EXPLICIT-REFS:ALanguagewithExplicitReferences |     |     |     |     |     |     | 109 |
| ------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
This rule says that newref-exp evaluates its operand. It extends the
resulting store by allocating a new location l and puts the value val of its
argumentin that location. Then it returnsa referenceto a location l that is
| new. Thismeansthatitisnotalreadyinthedomainof |     |     |     |     |     | σ . |     |
| --------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
1
|     |           |            |     | ρ σ )=(l,σ |          |         |     |
| --- | --------- | ---------- | --- | ---------- | -------- | ------- | --- |
|     |           | (value-of  | exp |            |          | )       |     |
|     |           |            |     | 0          |          | 1       |     |
|     | (value-of | (deref-exp |     | exp)       | ρ σ )=(σ | (l),σ ) |     |
|     |           |            |     |            | 0        | 1 1     |     |
This rule says thata deref-expevaluatesits operand, leaving the store
in state σ . The value of that argument should be a reference to a location
1
σ
l. The deref-expthenreturnsthe contentsof l in , without anyfurther
1
changetothestore.
|           |     |             |     | ρ σ )=(l,σ   |                                    |     |     |
| --------- | --- | ----------- | --- | ------------ | ---------------------------------- | --- | --- |
|           |     | (value-of   | exp |              |                                    | )   |     |
|           |     |             |     | 1 0          |                                    | 1   |     |
|           |     | (value-of   | exp | ρ σ )=(val,σ |                                    | )   |     |
|           |     |             |     | 2 1          |                                    | 2   |     |
| (value-of |     | (setref-exp | exp | exp )        | ρ σ )=((cid:13)23(cid:14),[l=val]σ |     | )   |
|           |     |             |     | 1 2          | 0                                  |     | 2   |
Thisrulesaysthatasetref-expevaluatesitsoperandsfromlefttoright.
The value of the first operand must be a reference to a location l. The
setref-exp then updates the resulting store by putting the value val of
the second argument in location l. What should a setref-exp return? It
couldreturnanything. Toemphasizethearbitrarynatureofthischoice,we
havespecifiedthatitreturns23. Becausewearenotinterestedinthe value
returnedbyasetref-exp,wesaythatthisexpressionisexecutedforeffect,
ratherthanforitsvalue.
(cid:3)
Exercise4.6 [ ]Modifytherulegivenabovesothatasetref-expreturnsthevalue
oftheright-handside.
(cid:3)
Exercise4.7 [ ]Modifytherulegivenabovesothatasetref-expreturnstheold
contentsofthelocation.
4.2.3 Implementation
Thespecificationlanguagewehaveusedsofarmakesiteasytodescribethe
desiredbehaviorofeffectfulcomputations,butitdoesnotembodyakeyfact
aboutthe store: a referenceultimatelyrefersto areallocation in a memory
thatexistsintherealworld. Sincewehaveonlyonerealworld,ourprogram
σ
| canonlykeeptrackofonestate |     |     | ofthestore. |     |     |     |     |
| -------------------------- | --- | --- | ----------- | --- | --- | --- | --- |
In our implementations, we take advantage of this fact by modeling the
storeusingScheme’sownstore. ThuswemodelaneffectasaSchemeeffect.

110 4 State
WerepresentthestateofthestoreasaSchemevalue,butwedonotexplic-
itly pass and return it, as the specification suggests. Instead, we keep the
stateinasingleglobalvariable,towhichalltheproceduresoftheimplemen-
tation have access. This is much like even/odd example, where we used
a shared location instead of passing an explicit argument. By using a sin-
gleglobalvariable,we alsouse aslittle aspossible ofour understandingof
Schemeeffects.
We still have to choose how to model the store as a Scheme value. We
choose the simplest possible model: we represent the store as a list of
expressedvalues,andareferenceisanumberthatdenotesapositioninthe
list. A newreferenceisallocatedbyappendinganewvaluetothe list; and
updatingthestoreismodeledbycopyingoverasmuchofthelistasneces-
sary.Thecodeisshowninfigures4.1and4.2.
Thisrepresentationisextremelyinefficient. Ordinarymemoryoperations
requireapproximatelyconstanttime,butinourrepresentationtheseopera-
tionsrequiretimeproportionaltothesizeofthestore. Norealimplementa-
tionwouldeverdothis,ofcourse,butitsufficesforourpurposes.
We add a new variant, ref-val, to the data type for expressed values,
andwemodifyvalue-of-programtoinitializethestorebeforeeacheval-
uation.
| value-of-program | : Program        | → ExpVal |     |     |
| ---------------- | ---------------- | -------- | --- | --- |
| (define          | value-of-program |          |     |     |
| (lambda          | (pgm)            |          |     |     |
(initialize-store!)
| (cases | program             | pgm             |                |         |
| ------ | ------------------- | --------------- | -------------- | ------- |
|        | (a-program (exp1)   |                 |                |         |
|        | (value-of exp1      | (init-env)))))) |                |         |
|        |                     | value-offor     | newref, deref, | setref. |
| Now we | can write clausesin |                 |                | and     |
Theclausesareshowninfigure4.3.
We can instrument our system by adding some procedures that convert
environments, procedures, and storestoa more readableform, and we can
instrument our system by printing messages at key points in the code. We
alsouse proceduresthat convertenvironments, procedures, and storesto a
morereadableform. Theresultinglogsgiveadetailedpictureofoursystem
inaction. Atypicalexampleisshowninfigures4.4and4.5. Thistraceshows,
amongotherthings,thattheargumentstothesubtractionareevaluatedfrom
lefttoright.

4.2 EXPLICIT-REFS:ALanguagewithExplicitReferences 111
empty-store : () → Sto
(define empty-store
(lambda () ’()))
usage: A Scheme variable containing the current state
of the store. Initially set to a dummy value.
(define the-store ’uninitialized)
get-store : () → Sto
(define get-store
(lambda () the-store))
initialize-store! : () → Unspecified
usage: (initialize-store!) sets the-store to the empty store
(define initialize-store!
(lambda ()
(set! the-store (empty-store))))
reference? : SchemeVal → Bool
(define reference?
(lambda (v)
(integer? v)))
newref : ExpVal → Ref
(define newref
(lambda (val)
(let ((next-ref (length the-store)))
(set! the-store (append the-store (list val)))
next-ref)))
deref : Ref → ExpVal
(define deref
(lambda (ref)
(list-ref the-store ref)))
Figure4.1 Anaivemodelofthestore

112 4 State
|          | ×         |            | →           |                    |          |
| -------- | --------- | ---------- | ----------- | ------------------ | -------- |
| setref!  | : Ref     | ExpVal     | Unspecified |                    |          |
| usage:   | sets      | the-store  | to a state  | like the original, | but with |
| position | ref       | containing | val.        |                    |          |
| (define  | setref!   |            |             |                    |          |
| (lambda  | (ref      | val)       |             |                    |          |
| (set!    | the-store |            |             |                    |          |
(letrec
((setref-inner
|     | usage:   | returns | a list        | like store1, except | that |
| --- | -------- | ------- | ------------- | ------------------- | ---- |
|     | position |         | ref1 contains | val.                |      |
|     | (lambda  |         | (store1 ref1) |                     |      |
(cond
|     |     | ((null?                   | store1)  |                 |     |
| --- | --- | ------------------------- | -------- | --------------- | --- |
|     |     | (report-invalid-reference |          | ref the-store)) |     |
|     |     | ((zero?                   | ref1)    |                 |     |
|     |     | (cons                     | val (cdr | store1)))       |     |
(else
(cons
|     |     |     | (car store1) |     |     |
| --- | --- | --- | ------------ | --- | --- |
(setref-inner
|     |               |     | (cdr store1)                    | (- ref1 1)))))))) |     |
| --- | ------------- | --- | ------------------------------- | ----------------- | --- |
|     | (setref-inner |     | the-store                       | ref)))))          |     |
|     | Figure4.2     |     | Anaivemodelofthestore,continued |                   |     |
(cid:3)
Exercise4.8 [ ]Showexactlywhereinourimplementationofthestoretheseopera-
tionstakelineartimeratherthanconstanttime.
(cid:3)
Exercise4.9 [ ]ImplementthestoreinconstanttimebyrepresentingitasaScheme
vector.Whatislostbyusingthisrepresentation?
| Exercise4.10 | [ (cid:3) ]Implementthebeginexpressionasspecifiedinexercise4.4. |     |     |     |     |
| ------------ | --------------------------------------------------------------- | --- | --- | --- | --- |
(cid:3)
| Exercise4.11 | [ ]Implementlistfromexercise4.5. |     |     |     |     |
| ------------ | -------------------------------- | --- | --- | --- | --- |
(cid:3)(cid:3)(cid:3)
Exercise4.12 [ ]Ourunderstandingofthestore,asexpressedinthisinterpreter,
dependsonthemeaningofeffectsinScheme.Inparticular,itdependsonusknowing
whentheseeffectstakeplaceinaSchemeprogram.Wecanavoidthisdependencyby
writinganinterpreterthatmorecloselymimicsthespecification. Inthisinterpreter,
value-ofwouldreturnbothavalueandastore,justasinthespecification. Afrag-
mentofthis interpreterappearsinfigure4.6. Wecallthis astore-passinginterpreter.
ExtendthisinterpretertocoverallofthelanguageEXPLICIT-REFS.
Everyprocedurethatmightmodifythestorereturnsnotjustitsusualvaluebutalsoa
newstore.Thesearepackagedinadatatypecalledanswer.Completethisdefinition
ofvalue-of.

4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences 113
| (newref-exp | (exp1)    |              |        |             |              |
| ----------- | --------- | ------------ | ------ | ----------- | ------------ |
| (let ((v1   | (value-of |              | exp1   | env)))      |              |
| (ref-val    |           | (newref      | v1)))) |             |              |
| (deref-exp  | (exp1)    |              |        |             |              |
| (let ((v1   | (value-of |              | exp1   | env)))      |              |
| (let        | ((ref1    | (expval->ref |        | v1)))       |              |
| (deref      |           | ref1))))     |        |             |              |
| (setref-exp | (exp1     | exp2)        |        |             |              |
| (let ((ref  |           | (expval->ref |        | (value-of   | exp1 env)))) |
| (let        | ((val2    | (value-of    |        | exp2 env))) |              |
(begin
|     | (setref! | ref     | val2) |     |     |
| --- | -------- | ------- | ----- | --- | --- |
|     | (num-val | 23))))) |       |     |     |
Figure4.3 value-ofclausesforexplicit-referenceoperators
Exercise4.13 [ (cid:3)(cid:3)(cid:3) ] Extendthe interpreterofthe precedingexerciseto have proce-
duresofmultiplearguments.
| 4.3 IMPLICIT-REFS:ALanguage |     |     | with | ImplicitReferences |     |
| --------------------------- | --- | --- | ---- | ------------------ | --- |
Theexplicitreferencedesigngivesaclearaccountofallocation, dereferenc-
ing, and mutation because all these operations are explicit in the program-
mer’scode.
Mostprogramminglanguagestakecommonpatternsofallocation,deref-
erencing,andmutation,andpackagethemupaspartofthelanguage. Then
the programmer need not worry about when to perform these operations,
becausetheyarebuiltintothelanguage.
Inthisdesign,everyvariabledenotesareference. Denotedvaluesareref-
erencestolocationsthatcontainexpressedvalues. Referencesarenolonger
expressedvalues. Theyexistonlyasthebindingsofvariables.
|     | ExpVal |     | = Int + | Bool + Proc |     |
| --- | ------ | --- | ------- | ----------- | --- |
=
|     | DenVal |     | Ref(ExpVal) |     |     |
| --- | ------ | --- | ----------- | --- | --- |
Locations are created with each binding operation: at each procedure call,
let,orletrec.

114 4 State
> (run "
| let x =  | newref(22)     |                            |
| -------- | -------------- | -------------------------- |
| in let f | = proc (z) let | zz = newref(-(z,deref(x))) |
in deref(zz)
| in -((f              | 66), (f 55))")      |              |
| -------------------- | ------------------- | ------------ |
| entering             | let x               |              |
| newref:              | allocating location | 0            |
| entering             | body of let         | x with env = |
| ((x #(struct:ref-val |                     | 0))          |
| (i #(struct:num-val  |                     | 1))          |
| (v #(struct:num-val  |                     | 5))          |
| (x #(struct:num-val  |                     | 10)))        |
store =
| ((0 #(struct:num-val |             | 22)))        |
| -------------------- | ----------- | ------------ |
| entering             | let f       |              |
| entering             | body of let | f with env = |
((f
(procedure
z
...
| ((x                 | #(struct:ref-val | 0))     |
| ------------------- | ---------------- | ------- |
| (i                  | #(struct:num-val | 1))     |
| (v                  | #(struct:num-val | 5))     |
| (x                  | #(struct:num-val | 10))))) |
| (x #(struct:ref-val |                  | 0))     |
| (i #(struct:num-val |                  | 1))     |
| (v #(struct:num-val |                  | 5))     |
| (x #(struct:num-val |                  | 10)))   |
store =
| ((0 #(struct:num-val |              | 22)))        |
| -------------------- | ------------ | ------------ |
| entering             | body of proc | z with env = |
| ((z #(struct:num-val |              | 66))         |
| (x #(struct:ref-val  |              | 0))          |
| (i #(struct:num-val  |              | 1))          |
| (v #(struct:num-val  |              | 5))          |
| (x #(struct:num-val  |              | 10)))        |
store =
| ((0 #(struct:num-val |                                               | 22))) |
| -------------------- | --------------------------------------------- | ----- |
|                      | Figure4.4 TraceofanevaluationinEXPLICIT-REFS. |       |

4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences 115
| entering let          | zz       |               |     |
| --------------------- | -------- | ------------- | --- |
| newref: allocating    | location | 1             |     |
| entering body         | of let   | zz with env = |     |
| ((zz #(struct:ref-val |          | 1))           |     |
| (z #(struct:num-val   |          | 66))          |     |
| (x #(struct:ref-val   |          | 0))           |     |
| (i #(struct:num-val   |          | 1))           |     |
| (v #(struct:num-val   |          | 5))           |     |
| (x #(struct:num-val   |          | 10)))         |     |
store =
| ((0 #(struct:num-val |         | 22)) (1 #(struct:num-val | 44))) |
| -------------------- | ------- | ------------------------ | ----- |
| entering body        | of proc | z with env =             |       |
| ((z #(struct:num-val |         | 55))                     |       |
| (x #(struct:ref-val  |         | 0))                      |       |
| (i #(struct:num-val  |         | 1))                      |       |
| (v #(struct:num-val  |         | 5))                      |       |
| (x #(struct:num-val  |         | 10)))                    |       |
store =
| ((0 #(struct:num-val  |          | 22)) (1 #(struct:num-val | 44))) |
| --------------------- | -------- | ------------------------ | ----- |
| entering let          | zz       |                          |       |
| newref: allocating    | location | 2                        |       |
| entering body         | of let   | zz with env =            |       |
| ((zz #(struct:ref-val |          | 2))                      |       |
| (z #(struct:num-val   |          | 55))                     |       |
| (x #(struct:ref-val   |          | 0))                      |       |
| (i #(struct:num-val   |          | 1))                      |       |
| (v #(struct:num-val   |          | 5))                      |       |
| (x #(struct:num-val   |          | 10)))                    |       |
store =
| ((0 #(struct:num-val |     | 22))  |     |
| -------------------- | --- | ----- | --- |
| (1 #(struct:num-val  |     | 44))  |     |
| (2 #(struct:num-val  |     | 33))) |     |
| #(struct:num-val     | 11) |       |     |
>
| Figure4.5 | TraceofanevaluationinEXPLICIT-REFS,continued |     |     |
| --------- | -------------------------------------------- | --- | --- |

116 4 State
(define-datatype answer answer?
(an-answer
(val expval?)
(store store?)))
value-of : Exp × Env × Sto → ExpVal
(define value-of
(lambda (exp env store)
(cases expression exp
(const-exp (num)
(an-answer (num-val num) store))
(var-exp (var)
(an-answer
(apply-store store (apply-env env var))
store))
(if-exp (exp1 exp2 exp3)
(cases answer (value-of exp1 env store)
(an-answer (val new-store)
(if (expval->bool val)
(value-of exp2 env new-store)
(value-of exp3 env new-store)))))
(deref-exp (exp1)
(cases answer (value-of exp1 env store)
(an-answer (v1 new-store)
(let ((ref1 (expval->ref v1)))
(an-answer (deref ref1) new-store)))))
...)))
Figure4.6 Store-passinginterpreterforexercise4.12
Whenavariableappearsinanexpression,wefirstlookuptheidentifierin
theenvironmenttofindthelocationtowhichitisbound,andthenwelook
upinthestoretofindthevalueatthatlocation. Hencewehavea“two-level”
systemforvar-exp.
Thecontentsofalocationcanbechangedbyasetexpression. Weusethe
syntax
Expression::=set Identifier = Expression
assign-exp (var exp1)
Here the Identifier is not part of an expression, so it does not get derefer-
enced.Inthisdesign,wesaythatvariablesaremutable,meaningchangeable.

4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences 117
let x = 0
in letrec even(dummy)
= if zero?(x)
then 1
else begin
|     | set  | x = -(x,1); |     |
| --- | ---- | ----------- | --- |
|     | (odd | 888)        |     |
end
odd(dummy)
= if zero?(x)
then 0
else begin
|     | set   | x = -(x,1); |     |
| --- | ----- | ----------- | --- |
|     | (even | 888)        |     |
end
| in begin    | set x = 13;  | (odd -888) | end |
| ----------- | ------------ | ---------- | --- |
| let g = let | count = 0    |            |     |
| in          | proc (dummy) |            |     |
begin
|     | set count | = -(count,-1); |     |
| --- | --------- | -------------- | --- |
count
end
| in let a = | (g 11)   |     |     |
| ---------- | -------- | --- | --- |
| in let b   | = (g 11) |     |     |
in -(a,b)
| Figure4.7 | oddandeveninIMPLICIT-REFS |     |     |
| --------- | ------------------------- | --- | --- |
Thisdesigniscalledcall-by-value,orimplicitreferences. Mostprogramming
languages,includingScheme,usesomevariationonthisdesign.
Figure4.7hasourtwosampleprogramsinthisdesign. Becausereferences
arenolongerexpressedvalues,wecan’tmakechainsofreferences,aswedid
inthelastexampleinsection4.2.
4.3.1 Specification
Wecanwritetherulesfordereferenceandseteasily. Theenvironmentnow
alwaysbindsvariablestolocations,sowhenavariableappearsasanexpres-
sion,weneedtodereferenceit:
| (value-of | (var-exp | var) ρ σ)=(σ | ( ρ (var)),σ) |
| --------- | -------- | ------------ | ------------- |

| 118 |     |     |     |     |     |     |     | 4 State |
| --- | --- | --- | --- | --- | --- | --- | --- | ------- |
Assignment works as one might expect: we look up the left-hand side
in the environment, getting a location, we evaluate the right-hand side in
theenvironment,andwemodifythedesiredlocation. Aswithsetref,the
valuereturnedbyasetexpressionisarbitrary. Wechoosetohaveitreturn
theexpressedvalue27.
|           |             | (value-of |     | ρ σ   | )=(val                      | ,σ  | )     |           |
| --------- | ----------- | --------- | --- | ----- | --------------------------- | --- | ----- | --------- |
|           |             |           | exp | 1     | 0                           | 1 1 |       |           |
| (value-of | (assign-exp |           |     | )     | ρ σ )=((cid:13)27(cid:14),[ |     | ρ =   | σ )       |
|           |             |           | var | exp 1 | 0                           |     | (var) | val 1 ] 1 |
We alsoneed torewritethe rules forprocedurecalland letto show the
| modifiedstore.                    |           | Forprocedurecall,therulebecomes |             |           |      |     |     |     |
| --------------------------------- | --------- | ------------------------------- | ----------- | --------- | ---- | --- | --- | --- |
| (apply-procedure                  |           |                                 | (procedure  |           |      | ρ)  | σ)  |     |
|                                   |           |                                 |             | var       | body | val |     |     |
| =                                 | (value-of | body                            | [var = l] ρ | [l = val] | σ)   |     |     |     |
| wherelisalocationnotinthedomainof |           |                                 |             |           | σ .  |     |     |     |
The rule for (let-exp var exp body) is similar. The right-hand side
1
let
exp 1 is evaluated, and the value of the expression is the value of the
body,evaluatedinanenvironmentwherethevariablevarisboundtoanew
| locationcontainingthevalueofexp |                                |     |     | 1 . |     |     |     |     |
| ------------------------------- | ------------------------------ | --- | --- | --- | --- | --- | --- | --- |
| Exercise4.14                    | [ (cid:3) ]Writetheruleforlet. |     |     |     |     |     |     |     |
4.3.2 TheImplementation
Nowwearereadytomodifythe interpreter. In value-of, wedereference
ateachvar-exp,justliketherulessay
|     | (var-exp | (var) | (deref | (apply-env |     | env var))) |     |     |
| --- | -------- | ----- | ------ | ---------- | --- | ---------- | --- | --- |
andwewritetheobviouscodeforaassign-exp
|     | (assign-exp |     | (var exp1) |     |     |     |     |     |
| --- | ----------- | --- | ---------- | --- | --- | --- | --- | --- |
(begin
(setref!
|     |     | (apply-env | env   | var)  |     |     |     |     |
| --- | --- | ---------- | ----- | ----- | --- | --- | --- | --- |
|     |     | (value-of  | exp1  | env)) |     |     |     |     |
|     |     | (num-val   | 27))) |       |     |     |     |     |
What about creating references? New locations should be allocated at
everynewbinding. Thereareexactlyfourplacesinthelanguagewherenew
bindingsarecreated:intheinitialenvironment,inalet,inaprocedurecall,
andinaletrec.

4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences 119
Intheinitialenvironment,weexplicitlyallocatenewlocations.
Forlet,wechangethecorrespondinglineinvalue-oftoallocateanew
locationcontainingthevalue,andtobindthevariabletoareferencetothat
location.
(let-exp (var exp1 body)
(let ((val1 (value-of exp1 env)))
(value-of body
(extend-env var (newref val1) env))))
For a procedure call, we similarly change apply-procedure to call
newref.
apply-procedure : Proc × ExpVal → ExpVal
(define apply-procedure
(lambda (proc1 val)
(cases proc proc1
(procedure (var body saved-env)
(value-of body
(extend-env var (newref val) saved-env))))))
Last, to handle letrec, we replace the extend-env-rec clause in
apply-env to return a reference to a location containing the appropri-
ate closure. Since we are using multideclaration letrec (exercise 3.32),
extend-env-rec takes a list of procedure names, a list of bound vari-
ables, a list of procedure bodies, and a saved environment. The procedure
locationtakesavariableandalistofvariablesandreturnseithertheposi-
tionofthevariableinthelist,or#fifitisnotpresent.
(extend-env-rec (p-names b-vars p-bodies saved-env)
(let ((n (location search-var p-names)))
(if n
(newref
(proc-val
(procedure
(list-ref b-vars n)
(list-ref p-bodies n)
env)))
(apply-env saved-env search-var))))
Figure 4.8 shows a simple evaluation in IMPLICIT-REFS, using the same
instrumentationasbefore.

120 4 State
| > (run  | "             |     |     |
| ------- | ------------- | --- | --- |
| let f = | proc (x) proc | (y) |     |
begin
|     | set x = -(x,-1); |     |     |
| --- | ---------------- | --- | --- |
-(x,y)
end
| in ((f   | 44) 33)")   |              |     |
| -------- | ----------- | ------------ | --- |
| newref:  | allocating  | location 0   |     |
| newref:  | allocating  | location 1   |     |
| newref:  | allocating  | location 2   |     |
| entering | let f       |              |     |
| newref:  | allocating  | location 3   |     |
| entering | body of let | f with env = |     |
| ((f 3)   | (i 0) (v 1) | (x 2))       |     |
store =
| ((0 #(struct:num-val |              | 1))          |           |
| -------------------- | ------------ | ------------ | --------- |
| (1 #(struct:num-val  |              | 5))          |           |
| (2 #(struct:num-val  |              | 10))         |           |
| (3 (procedure        | x ...        | ((i 0) (v 1) | (x 2))))) |
| newref:              | allocating   | location 4   |           |
| entering             | body of proc | x with env = |           |
| ((x 4)               | (i 0) (v 1)  | (x 2))       |           |
store =
| ((0 #(struct:num-val |              | 1))          |          |
| -------------------- | ------------ | ------------ | -------- |
| (1 #(struct:num-val  |              | 5))          |          |
| (2 #(struct:num-val  |              | 10))         |          |
| (3 (procedure        | x ...        | ((i 0) (v 1) | (x 2)))) |
| (4 #(struct:num-val  |              | 44)))        |          |
| newref:              | allocating   | location 5   |          |
| entering             | body of proc | y with env = |          |
| ((y 5)               | (x 4) (i 0)  | (v 1) (x 2)) |          |
store =
| ((0 #(struct:num-val |       | 1))          |          |
| -------------------- | ----- | ------------ | -------- |
| (1 #(struct:num-val  |       | 5))          |          |
| (2 #(struct:num-val  |       | 10))         |          |
| (3 (procedure        | x ... | ((i 0) (v 1) | (x 2)))) |
| (4 #(struct:num-val  |       | 44))         |          |
| (5 #(struct:num-val  |       | 33)))        |          |
| #(struct:num-val     | 12)   |              |          |
>
|     | Figure4.8 | SampleevaluationinIMPLICIT-REFS |     |
| --- | --------- | ------------------------------- | --- |

4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences 121
(cid:3)
Exercise4.15 [ ]Infigure4.8,whyarevariablesintheenvironmentboundtoplain
integersratherthanexpressedvalues,asinfigure4.5?
(cid:3)
Exercise4.16 [ ] Nowthatvariablesaremutable,wecanbuildrecursiveprocedures
byassignment.Forexample
letrec times4(x) = if zero?(x)
then 0
else -((times4 -(x,1)), -4)
in (times4 3)
canbereplacedby
let times4 = 0
in begin
set times4 = proc (x)
if zero?(x)
then 0
else -((times4 -(x,1)), -4);
(times4 3)
end
Tracethisbyhandandverifythatthistranslationworks.
(cid:3)(cid:3)
Exercise4.17 [ ]Writetherulesforandimplementmultiargumentproceduresand
letexpressions.
(cid:3)(cid:3)
Exercise4.18 [ ]Writetheruleforandimplementmultiprocedureletrecexpres-
sions.
(cid:3)(cid:3)
Exercise4.19 [ ]Modify the implementation of multiprocedure letrec so that
eachclosureisbuiltonlyonce, andonlyonelocationisallocatedforit. Thisislike
exercise3.35.
(cid:3)(cid:3)
Exercise4.20 [ ]Inthelanguageofthissection,allvariablesaremutable, asthey
areinScheme. Anotheralternativeistoallowbothmutableandimmutablevariable
bindings:
ExpVal = Int+Bool+Proc
DenVal = Ref(ExpVal)+ExpVal
Variable assignment should work only when the variable to be assigned to has a
mutablebinding. Dereferencingoccursimplicitlywhenthedenotedvalueisarefer-
ence.
Modifythelanguageofthissectionsothat letintroducesimmutablevariables,as
before,butmutablevariablesareintroducedbyaletmutableexpression,withsyn-
taxgivenby
Expression::=letmutable Identifier = Expression in Expression

122 4 State
(cid:3)(cid:3)
Exercise4.21 [ ] We suggested earlier the use of assignment to make a program
moremodularbyallowingoneproceduretocommunicate informationtoadistant
procedurewithout requiringintermediateproceduresto be aware of it. Veryoften
such an assignment should only be temporary, lasting for the execution of a pro-
cedurecall. Add to the language a facility for dynamic assignment (also called fluid
binding)toaccomplishthis.Usetheproduction
Expression::=setdynamic Identifier = Expression during Expression
setdynamic-exp (var exp body)
1
Theeffectofthesetdynamicexpressionistoassigntemporarilythevalueofexp to
1
var,evaluatebody,reassignvartoitsoriginalvalue,andreturnthevalueofbody.The
variablevarmustalreadybebound.Forexample,in
let x = 11
in let p = proc (y) -(y,x)
in -(setdynamic x = 17 during (p 22),
(p 13))
thevalueofx,whichisfreeinprocedurep,is17inthecall(p 22),butisresetto11
inthecall(p 13),sothevalueoftheexpressionis5−2=3.
(cid:3)(cid:3)
Exercise4.22 [ ]Sofarourlanguageshavebeenexpression-oriented: theprimary
syntacticcategoryofinteresthasbeenexpressionsandwehaveprimarilybeeninter-
estedin their values. Extend the language to model the simplestatement-oriented
language whose specification is sketched below. Be sure to Follow the Grammar by
writingseparateprocedurestohandleprograms,statements,andexpressions.
Values AsinIMPLICIT-REFS.
Syntax Usethefollowingsyntax:
Program ::=Statement
Statement::=Identifier = Expression
::=print Expression
::={{Statement}∗(;)
}
::=if Expression Statement Statement
::=while Expression Statement
::=var {Identifier}∗(,) ; Statement
The nonterminal Expression refers to the language of expressions of IMPLICIT-
REFS,perhapswithsomeextensions.
Semantics Aprogramisastatement. Astatementdoesnotreturnavalue,butacts
bymodifyingthestoreandbyprinting.
Assignment statements work in the usual way. A print statement evaluates its
actualparameterandprintstheresult. Theifstatementworksintheusualway.
Ablockstatement,definedinthelastproductionforStatement,bindseachofthe

| 4.3 IMPLICIT-REFS:ALanguagewithImplicitReferences |     |     |     |     |     | 123 |
| ------------------------------------------------- | --- | --- | --- | --- | --- | --- |
declaredvariablestoanuninitializedreferenceandthenexecutesthebodyofthe
block.Thescopeofthesebindingsisthebody.
Writethespecificationforstatementsusingassertionslike
|     |     |            | ρ    | σ )=σ |     |     |
| --- | --- | ---------- | ---- | ----- | --- | --- |
|     |     | (result-of | stmt |       |     |     |
0 1
| Examples | Herearesomeexamples. |         |              |           |           |     |
| -------- | -------------------- | ------- | ------------ | --------- | --------- | --- |
| (run     | "var x,y;            | {x = 3; | y = 4; print | +(x,y)}") | % Example | 1   |
7
| (run | "var x,y,z; | {x = | 3;  |     | % Example | 2   |
| ---- | ----------- | ---- | --- | --- | --------- | --- |
y = 4;
z = 0;
|     |     | while | not(zero?(x)) |              |     |     |
| --- | --- | ----- | ------------- | ------------ | --- | --- |
|     |     | {z    | = +(z,y);     | x = -(x,1)}; |     |     |
|     |     | print | z}")          |              |     |     |
12
| (run | " var x; | {x = 3; |     |     | % Example | 3   |
| ---- | -------- | ------- | --- | --- | --------- | --- |
print x;
|     |     | var x; | {x = 4; print | x}; |     |     |
| --- | --- | ------ | ------------- | --- | --- | --- |
print x}")
3
4
3
| (run | "var f,x; | {f = proc(x,y) | *(x,y); |     | % Example | 4   |
| ---- | --------- | -------------- | ------- | --- | --------- | --- |
x = 3;
|     |     | print | (f 4 x)}") |     |     |     |
| --- | --- | ----- | ---------- | --- | --- | --- |
12
Example3illustratesthescopingoftheblockstatement.
Example4illustratestheinteractionbetweenstatementsandexpressions. Apro-
cedurevalueiscreatedandstoredinthevariablef.Inthelastline,thisprocedure
isappliedtothe actualparameters4and x; sincexisboundtoareference,itis
dereferencedtoobtain3.
|              | (cid:3)     |          | ofexercise4.22readstatements |     |       |      |
| ------------ | ----------- | -------- | ---------------------------- | --- | ----- | ---- |
| Exercise4.23 | [ ]Addtothe | language |                              |     | ofthe | form |
readvar. Thisstatementreadsanonnegativeintegerfromtheinputandstoresitin
thegivenvariable.
Exercise4.24 [ (cid:3) ] A do-while statement is likea while statement, exceptthat the
testis performedafter the executionof the body. Add do-whilestatements to the
languageofexercise4.22.
(cid:3)
Exercise4.25 [ ] Extendtheblockstatementofthelanguageofexercise4.22toallow
variablestobeinitialized. Inyoursolution,doesthescopeofavariableincludethe
initializerforvariablesdeclaredlaterinthesameblockstatement?

124 4 State
(cid:3)(cid:3)(cid:3)
Exercise4.26 [ ]Extendthesolutiontotheprecedingexercisesothatprocedures
declaredinasingleblockaremutuallyrecursive.Considerrestrictingthelanguageso
thatthevariabledeclarationsinablockarefollowedbytheproceduredeclarations.
(cid:3)(cid:3)
Exercise4.27 [ ]Extendthe languageoftheprecedingexercisetoincludesubrou-
tines. Inourusageasubroutineislikeaprocedure,exceptthatitdoesnotreturna
value and its bodyisastatement, rather than an expression. Also, add subroutine
callsasanewkindofstatementandextendthesyntaxofblockssothattheymaybe
usedtodeclarebothproceduresandsubroutines. Howdoesthisaffectthedenoted
andexpressedvalues?Whathappensifaprocedureisreferencedinasubroutinecall,
orviceversa?
| 4.4 | MUTABLE-PAIRS:A |     | Languagewith | Mutable Pairs |
| --- | --------------- | --- | ------------ | ------------- |
In exercise 3.9 we added lists to our language, but these were immutable:
therewasnothinglikeScheme’sset-car!orset-cdr!forthem.
Now, let’s add mutable pairs to IMPLICIT-REFS. Pairs will be expressed
values,andwillhavethefollowingoperations:
|     |         | ×         | →              |     |
| --- | ------- | --------- | -------------- | --- |
|     | newpair | : Expval  | Expval MutPair |     |
|     | left    | : MutPair | → Expval       |     |
→
|     | right    | : MutPair | Expval                 |     |
| --- | -------- | --------- | ---------------------- | --- |
|     | setleft  | : MutPair | × Expval → Unspecified |     |
|     |          |           | × →                    |     |
|     | setright | : MutPair | Expval Unspecified     |     |
Apairconsistsoftwolocations,eachofwhichisindependentlyassignable.
Thisgivesusthedomainequations:
|     |     |         | = +           | + +          |
| --- | --- | ------- | ------------- | ------------ |
|     |     | ExpVal  | Int Bool      | Proc MutPair |
|     |     | DenVal  | = Ref(ExpVal) |              |
|     |     |         | =             | ×            |
|     |     | MutPair | Ref(ExpVal)   | Ref(ExpVal)  |
WecallthislanguageMUTABLE-PAIRS.
4.4.1 Implementation
We can implement this literally using the reference data type from our
|     | precedingexamples. | Thecodeisshowninfigure4.9. |     |     |
| --- | ------------------ | -------------------------- | --- | --- |
Once we’ve done this, it is straightforward to add these to the language.
We add a mutpair-valvariant to our data type of expressed values, and
fivenewlinestovalue-of. Theseareshown infigure4.10. Wearbitrarily
choosetomakesetleftreturn82andsetrightreturn83.Thetraceofan
example,usingthesameinstrumentationasbefore,isshowninfigures4.11
and4.12.

4.4 MUTABLE-PAIRS:ALanguagewithMutablePairs 125
| (define-datatype | mutpair | mutpair? |
| ---------------- | ------- | -------- |
(a-pair
| (left-loc  | reference?)     |         |
| ---------- | --------------- | ------- |
| (right-loc | reference?)))   |         |
|            | ×               | →       |
| make-pair  | : ExpVal ExpVal | MutPair |
| (define    | make-pair       |         |
| (lambda    | (val1 val2)     |         |
(a-pair
| (newref | val1)    |     |
| ------- | -------- | --- |
| (newref | val2)))) |     |
→
| left : MutPair | ExpVal    |            |
| -------------- | --------- | ---------- |
| (define        | left      |            |
| (lambda        | (p)       |            |
| (cases         | mutpair p |            |
| (a-pair        | (left-loc | right-loc) |
(deref left-loc)))))
| right : MutPair | → ExpVal  |            |
| --------------- | --------- | ---------- |
| (define         | right     |            |
| (lambda         | (p)       |            |
| (cases          | mutpair p |            |
| (a-pair         | (left-loc | right-loc) |
(deref right-loc)))))
| setleft :  | MutPair × ExpVal   | → Unspecified |
| ---------- | ------------------ | ------------- |
| (define    | setleft            |               |
| (lambda    | (p val)            |               |
| (cases     | mutpair p          |               |
| (a-pair    | (left-loc          | right-loc)    |
|            | (setref! left-loc  | val)))))      |
| setright : | MutPair × ExpVal   | → Unspecified |
| (define    | setright           |               |
| (lambda    | (p val)            |               |
| (cases     | mutpair p          |               |
| (a-pair    | (left-loc          | right-loc)    |
|            | (setref! right-loc | val)))))      |
Figure4.9 Naiveimplementationofmutablepairs

126 4 State
| (newpair-exp     | (exp1 exp2)      |      |               |
| ---------------- | ---------------- | ---- | ------------- |
| (let ((val1      | (value-of        | exp1 | env))         |
| (val2            | (value-of        | exp2 | env)))        |
| (mutpair-val     | (make-pair       |      | val1 val2)))) |
| (left-exp (exp1) |                  |      |               |
| (let ((val1      | (value-of        | exp1 | env)))        |
| (let ((p1        | (expval->mutpair |      | val1)))       |
| (left            | p1))))           |      |               |
| (right-exp       | (exp1)           |      |               |
| (let ((val1      | (value-of        | exp1 | env)))        |
| (let ((p1        | (expval->mutpair |      | val1)))       |
| (right           | p1))))           |      |               |
| (setleft-exp     | (exp1 exp2)      |      |               |
| (let ((val1      | (value-of        | exp1 | env))         |
| (val2            | (value-of        | exp2 | env)))        |
| (let ((p         | (expval->mutpair |      | val1)))       |
(begin
| (setleft      | p val2)          |      |         |
| ------------- | ---------------- | ---- | ------- |
| (num-val      | 82)))))          |      |         |
| (setright-exp | (exp1 exp2)      |      |         |
| (let ((val1   | (value-of        | exp1 | env))   |
| (val2         | (value-of        | exp2 | env)))  |
| (let ((p      | (expval->mutpair |      | val1))) |
(begin
| (setright                                            | p val2) |     |     |
| ---------------------------------------------------- | ------- | --- | --- |
| (num-val                                             | 83))))) |     |     |
| Figure4.10 Integratingmutablepairsintotheinterpreter |         |     |     |
4.4.2 AnotherRepresentationofMutablePairs
Therepresentationofamutablepairastworeferencesdoesnottakeadvan-
tageofallweknowaboutMutPair. Thetwolocationsinapairareindepen-
dently assignable, but they arenot independently allocated. We know that
theywillbeallocatedtogether: ifthe leftpartofapairisone location, then
therightpartisinthenextlocation. Sowecaninsteadrepresentthepairby
areferencetoitsleft. Thecodeforthisisshowninfigure4.13. Nothingelse
needchange.

4.4 MUTABLE-PAIRS:ALanguagewithMutablePairs 127
| > (run "let | glo = pair(11,22) |                   |     |            |
| ----------- | ----------------- | ----------------- | --- | ---------- |
| in let f    | = proc (loc)      |                   |     |            |
|             | let d1            | = setright(loc,   |     | left(loc)) |
|             | in let            | d2 = setleft(glo, |     | 99)        |
in -(left(loc),right(loc))
in (f glo)")
| ;; allocating      | cells       | for init-env |          |     |
| ------------------ | ----------- | ------------ | -------- | --- |
| newref: allocating |             | location     | 0        |     |
| newref: allocating |             | location     | 1        |     |
| newref: allocating |             | location     | 2        |     |
| entering           | let glo     |              |          |     |
| ;; allocating      | cells       | for the      | pair     |     |
| newref: allocating |             | location     | 3        |     |
| newref: allocating |             | location     | 4        |     |
| ;; allocating      | cell        | for glo      |          |     |
| newref: allocating |             | location     | 5        |     |
| entering           | body of let | glo          | with env | =   |
| ((glo 5)           | (i 0) (v 1) | (x 2))       |          |     |
store =
| ((0 #(struct:num-val    |             | 1))      |                 |         |
| ----------------------- | ----------- | -------- | --------------- | ------- |
| (1 #(struct:num-val     |             | 5))      |                 |         |
| (2 #(struct:num-val     |             | 10))     |                 |         |
| (3 #(struct:num-val     |             | 11))     |                 |         |
| (4 #(struct:num-val     |             | 22))     |                 |         |
| (5 #(struct:mutpair-val |             |          | #(struct:a-pair | 3 4)))) |
| entering                | let f       |          |                 |         |
| ;; allocating           | cell        | for f    |                 |         |
| newref: allocating      |             | location | 6               |         |
| entering                | body of let | f with   | env =           |         |
| ((f 6) (glo             | 5) (i 0)    | (v 1)    | (x 2))          |         |
store =
| ((0 #(struct:num-val    |            | 1))                              |                 |                    |
| ----------------------- | ---------- | -------------------------------- | --------------- | ------------------ |
| (1 #(struct:num-val     |            | 5))                              |                 |                    |
| (2 #(struct:num-val     |            | 10))                             |                 |                    |
| (3 #(struct:num-val     |            | 11))                             |                 |                    |
| (4 #(struct:num-val     |            | 22))                             |                 |                    |
| (5 #(struct:mutpair-val |            |                                  | #(struct:a-pair | 3 4)))             |
| (6 (procedure           | loc        | ... ((glo                        | 5) (i           | 0) (v 1) (x 2))))) |
|                         | Figure4.11 | TraceofevaluationinMUTABLE-PAIRS |                 |                    |

128 4 State
| ;; allocating      | cell for | loc            |     |
| ------------------ | -------- | -------------- | --- |
| newref: allocating | location | 7              |     |
| entering body      | of proc  | loc with env = |     |
| ((loc 7) (glo      | 5) (i 0) | (v 1) (x 2))   |     |
store =
| ((0 #(struct:num-val    | 1))     |                 |                |
| ----------------------- | ------- | --------------- | -------------- |
| (1 #(struct:num-val     | 5))     |                 |                |
| (2 #(struct:num-val     | 10))    |                 |                |
| (3 #(struct:num-val     | 11))    |                 |                |
| (4 #(struct:num-val     | 22))    |                 |                |
| (5 #(struct:mutpair-val |         | #(struct:a-pair | 3 4)))         |
| (6 (procedure           | loc ... | ((glo 5) (i 0)  | (v 1) (x 2)))) |
| (7 #(struct:mutpair-val |         | #(struct:a-pair | 3 4))))        |
| #(struct:num-val        | 88)     |                 |                |
>
Figure4.12 TraceofevaluationinMUTABLE-PAIRS,cont’d
Similarly,onecouldrepresentanyaggregateobjectintheheapbyapointer
toitsfirstlocation. However,apointerdoesnotbyitselfidentifyanareaof
memory unless it is supplemented by information about the length of the
area(seeexercise4.30). Thelackof lengthinformation isasourceofclassic
securityerrors,suchasout-of-boundsarraywrites.
(cid:3)(cid:3)
Exercise4.28 [ ]Writedownthespecificationrulesforthefivemutable-pairoper-
ations.
Exercise4.29 [ (cid:3)(cid:3) ]Addarraystothislanguage.Introducenewoperatorsnewarray,
arrayref,andarrayset thatcreate,dereference,andupdatearrays.Thisleadsto
|     | ArrVal=(Ref(ExpVal)) | ∗   |     |
| --- | -------------------- | --- | --- |
ExpVal=Int+Bool+Proc+ArrVal
DenVal=Ref(ExpVal)
Sincethelocationsinanarrayareconsecutive,usearepresentationlikethesecond
representationabove.Whatshouldbetheresultofthefollowingprogram?

| 4.4 MUTABLE-PAIRS:ALanguagewithMutablePairs |     |     |     |     | 129 |
| ------------------------------------------- | --- | --- | --- | --- | --- |
→
| mutpair?    | : SchemeVal    | Bool    |         |     |     |
| ----------- | -------------- | ------- | ------- | --- | --- |
| (define     | mutpair?       |         |         |     |     |
| (lambda     | (v)            |         |         |     |     |
| (reference? | v)))           |         |         |     |     |
|             | : ×            |         | →       |     |     |
| make-pair   | ExpVal         | ExpVal  | MutPair |     |     |
| (define     | make-pair      |         |         |     |     |
| (lambda     | (val1 val2)    |         |         |     |     |
| (let        | ((ref1 (newref | val1))) |         |     |     |
| (let        | ((ref2         | (newref | val2))) |     |     |
ref1))))
| :            | →                   |                                       |             |     |     |
| ------------ | ------------------- | ------------------------------------- | ----------- | --- | --- |
| left MutPair | ExpVal              |                                       |             |     |     |
| (define      | left                |                                       |             |     |     |
| (lambda      | (p)                 |                                       |             |     |     |
| (deref       | p)))                |                                       |             |     |     |
| right :      | MutPair → ExpVal    |                                       |             |     |     |
| (define      | right               |                                       |             |     |     |
| (lambda      | (p)                 |                                       |             |     |     |
| (deref       | (+ 1 p))))          |                                       |             |     |     |
| setleft :    | MutPair × ExpVal    | →                                     | Unspecified |     |     |
| (define      | setleft             |                                       |             |     |     |
| (lambda      | (p val)             |                                       |             |     |     |
| (setref!     | p val)))            |                                       |             |     |     |
|              | ×                   | →                                     |             |     |     |
| setright     | : MutPair           | ExpVal                                | Unspecified |     |     |
| (define      | setright            |                                       |             |     |     |
| (lambda      | (p val)             |                                       |             |     |     |
| (setref!     | (+ 1                | p) val)))                             |             |     |     |
|              | Figure4.13          | Alternaterepresentationofmutablepairs |             |     |     |
| let          | a = newarray(2,-99) |                                       |             |     |     |
p = proc (x)
|     | let v | = arrayref(x,1) |     |     |     |
| --- | ----- | --------------- | --- | --- | --- |
in arrayset(x,1,-(v,-1))
| in  | begin arrayset(a,1,0); |     | (p a); | (p a); arrayref(a,1) | end |
| --- | ---------------------- | --- | ------ | -------------------- | --- |
Herenewarray(2,-99)isintendedtobuildanarrayofsize2,witheachlocation
inthearraycontaining-99. beginexpressionsaredefinedinexercise4.4. Makethe
arrayindiceszero-based,soanarrayofsize2hasindices0and1.

130 4 State
(cid:3)(cid:3)
Exercise4.30 [ ]Addtothelanguageofexercise4.29aprocedurearraylength,
which returns the size of an array. Your procedure should work in constant time.
Makesurethatarrayrefandarraysetchecktomakesurethattheirindicesare
withinthelengthofthearray.
4.5 Parameter-PassingVariations
When a procedure body is executed, its formal parameter is bound to a
denoted value. Where does thatvalue come from? It must be passed from
theactualparameterintheprocedurecall. Wehavealreadyseentwoways
inwhichaparametercanbepassed:
• Naturalparameterpassing,inwhichthedenotedvalueisthesameasthe
expressedvalueoftheactualparameter(page75).
• Call-by-value,inwhichthedenotedvalueisareferencetoalocationcon-
taining the expressedvalue of the actualparameter (section 4.3). This is
themostcommonlyusedformofparameter-passing.
In this section, we explore some alternative parameter-passing mecha-
nisms.
4.5.1 CALL-BY-REFERENCE
Considerthefollowingexpression:
let p = proc (x) set x = 4
in let a = 3
in begin (p a); a end
Undercall-by-value,thedenotedvalueassociatedwithxisareferencethat
initiallycontainsthesamevalueasthereferenceassociatedwitha,butthese
referencesaredistinct. Thustheassignmenttoxhasnoeffectonthecontents
ofa’sreference,sothevalueoftheentireexpressionis3.
With call-by-value, when a procedure assigns a new value to one of its
parameters,thiscannotpossiblybeseenbyitscaller.Ofcourse,iftheparam-
eter passed to the caller contains a mutable pair, as in section 4.4, then the
effectofsetleftorsetrightwillbevisibletoacaller. Buttheeffectofa
setisnot.
Thoughthisisolationbetweenthecallerandcalleeisgenerallydesirable,
there are times when it is valuable to allow a procedure to be passed loca-
tionswiththeexpectationthattheywillbeassignedbytheprocedure. This

4.5 Parameter-PassingVariations 131
maybeaccomplishedbypassingtheprocedureareferencetothelocationof
thecaller’svariable,ratherthanthecontentsofthevariable.Thisparameter-
passingmechanismiscalledcall-by-reference. Ifanoperandissimplyavari-
able reference, a reference to the variable’s location is passed. The formal
parameterofthe procedureisthenboundtothislocation. Iftheoperandis
someotherkindofexpression,thentheformalparameterisboundtoanew
locationcontainingthe valueoftheoperand,justasincall-by-value. Using
call-by-referenceintheaboveexample,theassignmentof4toxhastheeffect
ofassigning4toa,sotheentireexpressionwouldreturn4,not3.
Whenacall-by-referenceprocedureiscalledandtheactualparameterisa
variable, what is passed is the locationof that variable, rather than the con-
tentsofthatlocation,asincall-by-value.Forexample,consider
| let f =  | proc (x) set | x = 44 |
| -------- | ------------ | ------ |
| in let g | = proc (y)   | (f y)  |
| in let   | z = 55       |        |
| in       | begin (g z); | z end  |
Whentheproceduregiscalled,yisboundtothelocationofz,notthecon-
tents of that location. Similarly, when f is called, x becomes bound to that
Sox,y,andzwillallbeboundtothesamelocation,andthe
samelocation.
effectoftheset x = 44istosetthatlocationto44. Hencethevalueofthe
entireexpressionis44. Atraceofthe executionof thisexpressionisshown
infigures4.14and4.15; inthisexample,x,y,andzallwindupbound to
location5.
Atypicaluseofcall-by-referenceistoreturnmultiplevalues. Aprocedure
canreturnonevalueinthenormalwayandassignotherstoparametersthat
arepassedbyreference. For another sortof example, consider the problem
ofswappingthevaluesintwovariables:
| let swap | = proc (x) | proc (y) |
| -------- | ---------- | -------- |
|          | let temp   | = x      |
in begin
|     | set | x = y;   |
| --- | --- | -------- |
|     | set | y = temp |
end
| in let a | = 33      |     |
| -------- | --------- | --- |
| in let   | b = 44    |     |
| in       | begin     |     |
|          | ((swap a) | b); |
-(a,b)
end

132 4 State
Undercall-by-reference,thisswapsthevaluesofaandb,soitreturns11. If
thisprogramwererunwithourexistingcall-by-valueinterpreter,however,
itwouldreturn-11,becausetheassignmentsinsidetheswapprocedurethen
havenoeffectonvariablesaandb.
Undercall-by-reference,variablesstilldenotereferencestoexpressedval-
ues,justastheydidundercall-by-value:
= + +
ExpVal Int Bool Proc
=
DenVal Ref(ExpVal)
The only thing that changes is the allocation of new locations. Under
call-by-value,a newlocation is createdfor everyevaluationof anoperand;
undercall-by-reference,anewlocationiscreatedforeveryevaluationofan
operandotherthanavariable.
Thisiseasytoimplement. Thefunctionapply-proceduremustchange,
becauseitisnolongertruethatanewlocation isallocatedforeveryproce-
dure call. That responsibility must be moved upstream, to the call-exp
lineinvalue-of,whichwillhavetheinformationtomakethatdecision.
apply-procedure : Proc × Ref → ExpVal
(define apply-procedure
(lambda (proc1 val)
(cases proc proc1
(procedure (var body saved-env)
(value-of body
(extend-env var val saved-env))))))
We then modify the call-exp line in value-of, and introduce a new
functionvalue-of-operandthatmakesthenecessarydecision.
(call-exp (rator rand)
(let ((proc (expval->proc (value-of rator env)))
(arg (value-of-operand rand env)))
(apply-procedure proc arg)))
Theprocedurevalue-of-operandcheckstoseeiftheoperandisavari-
able.Ifitis,thenthereferencethatthevariabledenotesisreturnedandthen
passedtothe procedurebyapply-procedure. Otherwise, theoperandis
evaluated,andareferencetoanewlocationcontainingthatvalueisreturned.
value-of-operand : Exp × Env → Ref
(define value-of-operand
(lambda (exp env)
(cases expression exp
(var-exp (var) (apply-env env var))
(else
(newref (value-of exp env))))))

4.5 Parameter-PassingVariations 133
Wecouldmodifylettoworkinasimilarfashion,butwehavechosennot
todoso,sothatsomecall-by-valuefunctionalitywillremaininthelanguage.
Morethanonecall-by-referenceparametermayrefertothesamelocation,
asinthefollowingprogram.
let b = 3
in let p = proc (x) proc(y)
begin
set x = 4;
y
end
in ((p b) b)
Thisyields4sincebothxandyrefertothesamelocation,whichisthebind-
ing of b. This phenomenon is known as variable aliasing. Here x and y are
aliases(names)forthesamelocation. Generally,wedonotexpectanassign-
menttoonevariabletochangethevalueofanother,soaliasingmakesitvery
difficulttounderstandprograms.
(cid:3)
Exercise4.31 [ ]WriteoutthespecificationrulesforCALL-BY-REFERENCE.
(cid:3)
Exercise4.32 [ ]ExtendthelanguageCALL-BY-REFERENCEtohaveproceduresof
multiplearguments.
(cid:3)(cid:3)
Exercise4.33 [ ] Extend the language CALL-BY-REFERENCEto support call-by-
valueproceduresaswell.
(cid:3)
Exercise4.34 [ ] Addacall-by-referenceversionoflet,calledletref,tothelan-
guage.Writethespecificationandimplementit.
(cid:3)(cid:3)
Exercise4.35 [ ] Wecangetsomeofthebenefitsofcall-by-referencewithoutleav-
ingthecall-by-valueframework. ExtendthelanguageIMPLICIT-REFSbyaddinga
newexpression
Expression::=ref Identifier
ref-exp (var)
ThisdiffersfromthelanguageEXPLICIT-REFS,sincereferencesareonlyofvariables.
Thisallowsustowritefamiliarprogramssuchasswapwithinourcall-by-valuelan-
guage.Whatshouldbethevalueofthisexpression?
let a = 3
in let b = 4
in let swap = proc (x) proc (y)
let temp = deref(x)
in begin
setref(x,deref(y));
setref(y,temp)
end
in begin ((swap ref a) ref b); -(a,b) end
Herewehaveusedaversionofletwithmultipledeclarations(exercise3.16).What
aretheexpressedanddenotedvaluesofthislanguage?

134 4 State
| > (run  | "        |         |       |     |     |
| ------- | -------- | ------- | ----- | --- | --- |
| let f = | proc     | (x) set | x =   | 44  |     |
| in let  | g = proc | (y)     | (f y) |     |     |
| in let  | z = 55   |         |       |     |     |
in begin
(g z);
z
end")
| newref:  | allocating |        | location | 0   |     |
| -------- | ---------- | ------ | -------- | --- | --- |
| newref:  | allocating |        | location | 1   |     |
| newref:  | allocating |        | location | 2   |     |
| entering | let        | f      |          |     |     |
| newref:  | allocating |        | location | 3   |     |
| entering | body       | of let | f with   | env | =   |
| ((f 3)   | (i 0)      | (v 1)  | (x 2))   |     |     |
store =
| ((0 #(struct:num-val |            |        | 1))      |        |              |
| -------------------- | ---------- | ------ | -------- | ------ | ------------ |
| (1 #(struct:num-val  |            |        | 5))      |        |              |
| (2 #(struct:num-val  |            |        | 10))     |        |              |
| (3 (procedure        |            | x ...  | ((i      | 0) (v  | 1) (x 2))))) |
| entering             | let        | g      |          |        |              |
| newref:              | allocating |        | location | 4      |              |
| entering             | body       | of let | g with   | env    | =            |
| ((g 4)               | (f 3)      | (i 0)  | (v 1)    | (x 2)) |              |
store =
| ((0 #(struct:num-val |            |        | 1))      |       |                    |
| -------------------- | ---------- | ------ | -------- | ----- | ------------------ |
| (1 #(struct:num-val  |            |        | 5))      |       |                    |
| (2 #(struct:num-val  |            |        | 10))     |       |                    |
| (3 (procedure        |            | x ...  | ((i      | 0) (v | 1) (x 2))))        |
| (4 (procedure        |            | y ...  | ((f      | 3) (i | 0) (v 1) (x 2))))) |
| entering             | let        | z      |          |       |                    |
| newref:              | allocating |        | location | 5     |                    |
| entering             | body       | of let | z with   | env   | =                  |
| ((z 5)               | (g 4)      | (f 3)  | (i 0)    | (v 1) | (x 2))             |
store =
| ((0 #(struct:num-val |            |       | 1))                                 |       |                   |
| -------------------- | ---------- | ----- | ----------------------------------- | ----- | ----------------- |
| (1 #(struct:num-val  |            |       | 5))                                 |       |                   |
| (2 #(struct:num-val  |            |       | 10))                                |       |                   |
| (3 (procedure        |            | x ... | ((i                                 | 0) (v | 1) (x 2))))       |
| (4 (procedure        |            | y ... | ((f                                 | 3) (i | 0) (v 1) (x 2)))) |
| (5 #(struct:num-val  |            |       | 55)))                               |       |                   |
|                      | Figure4.14 |       | SampleevaluationinCALL-BY-REFERENCE |       |                   |

4.5 Parameter-PassingVariations 135
| entering |     | body  | of proc | y with   | env = |     |     |     |
| -------- | --- | ----- | ------- | -------- | ----- | --- | --- | --- |
| ((y      | 5)  | (f 3) | (i 0)   | (v 1) (x | 2))   |     |     |     |
store =
| ((0      | #(struct:num-val |       |         | 1))    |       |          |       |     |
| -------- | ---------------- | ----- | ------- | ------ | ----- | -------- | ----- | --- |
| (1       | #(struct:num-val |       |         | 5))    |       |          |       |     |
| (2       | #(struct:num-val |       |         | 10))   |       |          |       |     |
| (3       | (procedure       |       | x ...   | ((i 0) | (v 1) | (x 2)))) |       |     |
| (4       | (procedure       |       | y ...   | ((f 3) | (i 0) | (v 1) (x | 2)))) |     |
| (5       | #(struct:num-val |       |         | 55)))  |       |          |       |     |
| entering |                  | body  | of proc | x with | env = |          |       |     |
| ((x      | 5)               | (i 0) | (v 1)   | (x 2)) |       |          |       |     |
store =
| ((0              | #(struct:num-val |     |       | 1))    |       |          |       |     |
| ---------------- | ---------------- | --- | ----- | ------ | ----- | -------- | ----- | --- |
| (1               | #(struct:num-val |     |       | 5))    |       |          |       |     |
| (2               | #(struct:num-val |     |       | 10))   |       |          |       |     |
| (3               | (procedure       |     | x ... | ((i 0) | (v 1) | (x 2)))) |       |     |
| (4               | (procedure       |     | y ... | ((f 3) | (i 0) | (v 1) (x | 2)))) |     |
| (5               | #(struct:num-val |     |       | 55)))  |       |          |       |     |
| #(struct:num-val |                  |     | 44)   |        |       |          |       |     |
>
|     |     | Figure4.15 | SampleevaluationinCALL-BY-REFERENCE,cont’d |     |     |     |     |     |
| --- | --- | ---------- | ------------------------------------------ | --- | --- | --- | --- | --- |
(cid:3)
Exercise4.36 [ ] Mostlanguagessupportarrays,inwhichcasearrayreferencesare
generallytreatedlikevariablereferencesundercall-by-reference.Ifanoperandisan
arrayreference,thenthelocationreferredto,ratherthanitscontents,ispassedtothe
calledprocedure.Thisallows,forexample,aswapproceduretobeusedincommonly
occurringsituationsinwhichthevaluesintwoarrayelementsaretobeexchanged.
Addarrayoperatorslikethoseofexercise4.29tothecall-by-referencelanguageofthis
section,andextendvalue-of-operandtohandlethiscase,sothat,forexample,a
procedureapplicationlike
|     |     |     | ((swap | (arrayref | a i)) | (arrayref | a j)) |     |
| --- | --- | --- | ------ | --------- | ----- | --------- | ----- | --- |
willworkasexpected.Whatshouldhappeninthecaseof
|     | ((swap |     | (arrayref | a (arrayref |     | a i))) (arrayref |     | a j))? |
| --- | ------ | --- | --------- | ----------- | --- | ---------------- | --- | ------ |
(cid:3)(cid:3)
Exercise4.37 [ ] Call-by-value-resultisavariationoncall-by-reference. In call-by-
value-result,theactualparametermustbeavariable. Whenaparameterispassed,
theformalparameterisboundtoanewreferenceinitializedtothevalueoftheactual

136 4 State
parameter, just as incall-by-value. The procedurebodyis then executednormally.
Whentheprocedurebodyreturns,however,thevalueinthenewreferenceiscopied
backintothereferencedenotedbytheactualparameter. Thismaybemoreefficient
than call-by-referencebecause it can improve memorylocality. Implement call-by-
value-resultandwriteaprogramthatproducesdifferentanswersusingcall-by-value-
resultandcall-by-reference.
4.5.2 LazyEvaluation: CALL-BY-NAMEandCALL-BY-NEED
All the parameter-passing mechanisms we have discussed so far are eager:
theyalwaysfindavalueforeachoperand. Wenowturntoaverydifferent
formofparameterpassing,calledlazyevaluation. Underlazyevaluation,an
operandinaprocedurecallisnotevaluateduntilitisneededbytheproce-
durebody. Ifthebodyneverreferstotheparameter,thenthereisnoneedto
evaluateit.
Thiscanpotentiallyavoidnon-termination. Forexample,consider
letrec infinite-loop (x) = infinite-loop(-(x,-1))
in let f = proc (z) 11
in (f (infinite-loop 0))
Here infinite-loop is a procedure that, when called, never terminates.
fis aprocedurethat, whencalled, never refersto itsargumentandalways
returns11.Underanyofthemechanismsconsideredsofar,thisprogramwill
failtoterminate. Underlazyevaluation, however, thisprogramwill return
11,becausetheoperand(infinite-loop 1)isneverevaluated.
We now modify our language to use lazy evaluation. Under lazy evalu-
ation, we do not evaluate an operand expression until it is needed. There-
fore we associate the bound variable of a procedure with an unevaluated
operand. When the procedure body needs the value of its bound variable,
the associatedoperandis evaluated. We sometimes saythatthe operand is
frozenwhen it is passed unevaluated to the procedure, and that it is thawed
whentheprocedureevaluatesit.
Ofcoursewewillalsohavetoincludetheenvironmentinwhichthatpro-
cedureistobeevaluated.Todothis,weintroduceanewdatatypeofthunks.
Athunkconsistsofanexpressionandanenvironment.
(define-datatype thunk thunk?
(a-thunk
(exp1 expression?)
(env environment?)))
Whenaprocedureneedstousethevalueofitsboundvariable,itwillevalu-
atetheassociatedthunk.

4.5 Parameter-PassingVariations 137
Oursituationissomewhatmorecomplicated,becauseweneedtoaccom-
modate both lazy evaluation, effects, and eager evaluation (for let). We
thereforeletourdenotedvaluesbereferencestolocationscontainingeither
expressedvaluesorthunks.
= +
DenVal Ref(ExpVal Thunk)
= + +
ExpVal Int Bool Proc
Ourpolicyforallocatingnewlocationswillbesimilartotheoneweused
for call-by-reference: If the operand is a variable, then we pass its denota-
tion, whichisareference. Otherwise, wepassareferencetoanewlocation
containingathunkfortheunevaluatedargument.
value-of-operand : Exp × Env → Ref
(define value-of-operand
(lambda (exp env)
(cases expression exp
(var-exp (var) (apply-env env var))
(else
(newref (a-thunk exp env))))))
Whenweevaluateavar-exp,wefirstfindthelocationtowhichthevari-
ableisbound. Ifthelocationcontainsanexpressedvalue,thenthatvalueis
returnedasthevalueofthevar-exp.Ifitinsteadcontainsathunk,thenthe
thunk is evaluated, and that value is returned. This design is called call by
name.
(var-exp (var)
(let ((ref1 (apply-env env var)))
(let ((w (deref ref1)))
(if (expval? w)
w
(value-of-thunk w)))))
Theprocedurevalue-of-thunkisdefinedas
value-of-thunk : Thunk → ExpVal
(define value-of-thunk
(lambda (th)
(cases thunk th
(a-thunk (exp1 saved-env)
(value-of exp1 saved-env))))
Alternatively, once we find the value of the thunk, we can install that
expressedvalueinthesamelocation,sothatthethunkwillnotbeevaluated
again. Thisarrangementiscalledcallbyneed.

138 4 State
| (var-exp | (var)        |            |         |            |     |
| -------- | ------------ | ---------- | ------- | ---------- | --- |
| (let     | ((ref1       | (apply-env |         | env var))) |     |
| (let     | ((w          | (deref     | ref1))) |            |     |
|          | (if (expval? |            | w)      |            |     |
w
|     | (let | ((val1 | (value-of-thunk |     | w))) |
| --- | ---- | ------ | --------------- | --- | ---- |
(begin
|     |     | (setref! | ref1 | val1) |     |
| --- | --- | -------- | ---- | ----- | --- |
val1))))))
Thisisaninstanceofageneralstrategycalledmemoization.
An attraction of lazy evaluation in all its forms is that in the absence of
effects, it supports reasoning about programs in a particularly simple way.
Theeffectof aprocedurecallcanbemodeledbyreplacingthe callwiththe
body of the procedure, with every reference to a formal parameter in the
bodyreplacedbythecorrespondingoperand.Thisevaluationstrategyisthe
β
basisforthelambdacalculus,whereitiscalled -reduction.
Unfortunately, call-by-name and call-by-need make it difficult to deter-
mine the order of evaluation, which in turn is essential to understanding
aprogramwitheffects. Iftherearenoeffects,though, thisisnotaproblem.
Thuslazyevaluationispopularinfunctionalprogramminglanguages(those
withnoeffects),andrarelyfoundelsewhere.
(cid:3)
Exercise4.38 [ ] The example below shows a variation of exercise3.25 that works
undercall-by-need. Doestheoriginalprograminexercise3.25workundercall-by-
need?Whathappensiftheprogrambelowisrunundercall-by-value?Why?
| let makerec | =          | proc  | (f)      |               |        |
| ----------- | ---------- | ----- | -------- | ------------- | ------ |
|             |            | let   | d = proc | (x) (f        | (x x)) |
|             |            | in (f | (d d))   |               |        |
| in let      | maketimes4 | =     | proc     | (f)           |        |
|             |            |       | proc     | (x)           |        |
|             |            |       | if       | zero?(x)      |        |
|             |            |       | then     | 0             |        |
|             |            |       | else     | -((f -(x,1)), | -4)    |
| in let      | times4     | =     | (makerec | maketimes4)   |        |
| in          | (times4    | 3)    |          |               |        |
(cid:3)
Exercise4.39 [ ]Intheabsenceofeffects,call-by-nameandcall-by-needalwaysgive
thesameanswer.Constructanexampleinwhichcall-by-nameandcall-by-needgive
differentanswers.
(cid:3)
Exercise4.40 [ ] Modifyvalue-of-operandso that it avoids making thunks for
constantsandprocedures.
Exercise4.41 [ (cid:3)(cid:3) ]Writeoutthespecificationrulesforcall-by-nameandcall-by-need.
(cid:3)(cid:3)
Exercise4.42 [ ]Addalazylettothecall-by-needinterpreter.

5
Continuation-Passing
Interpreters
In chapter 3, we used the concept of environments to explore the behav-
ior of bindings, which establish the data context in which each portion of
a program is executed. Here we will do the same for the control context in
which each portion of a program is executed. We will introduce the con-
cept of a continuation as an abstraction of the control context, and we will
writeinterpretersthattakeacontinuation asanargument,thusmakingthe
controlcontextexplicit.
ConsiderthefollowingdefinitionofthefactorialfunctioninScheme.
| (define | fact   |      |               |           |
| ------- | ------ | ---- | ------------- | --------- |
| (lambda | (n)    |      |               |           |
| (if     | (zero? | n) 1 | (* n (fact (- | n 1)))))) |
Wecanuseaderivationtomodelacalculationwithfact:
| (fact  | 4)    |            |                |     |
| ------ | ----- | ---------- | -------------- | --- |
| = (* 4 | (fact | 3))        |                |     |
| = (* 4 | (* 3  | (fact 2))) |                |     |
| = (* 4 | (* 3  | (* 2 (fact | 1))))          |     |
| = (* 4 | (* 3  | (* 2 (*    | 1 (fact 0))))) |     |
| = (* 4 | (* 3  | (* 2 (*    | 1 1))))        |     |
| = (* 4 | (* 3  | (* 2 1)))  |                |     |
| = (* 4 | (* 3  | 2))        |                |     |
= (* 4 6)
= 24
This is the natural recursive definition of factorial. Each call of fact is
madewithapromisethatthevaluereturnedwillbemultipliedbythevalue
ofnatthetimeofthecall. Thusfactisinvokedinlargerandlargercontrol
contexts as the calculation proceeds. Compare this behavior to that of the
followingprocedures.

140 5 Continuation-PassingInterpreters
| (define fact-iter     |                     |         |             |
| --------------------- | ------------------- | ------- | ----------- |
| (lambda (n)           |                     |         |             |
| (fact-iter-acc        | n 1)))              |         |             |
| (define fact-iter-acc |                     |         |             |
| (lambda (n            | a)                  |         |             |
| (if (zero?            | n) a (fact-iter-acc | (- n 1) | (* n a))))) |
Withthesedefinitions,wecalculate:
| (fact-iter       | 4)    |     |     |
| ---------------- | ----- | --- | --- |
| = (fact-iter-acc | 4 1)  |     |     |
| = (fact-iter-acc | 3 4)  |     |     |
| = (fact-iter-acc | 2 12) |     |     |
| = (fact-iter-acc | 1 24) |     |     |
| = (fact-iter-acc | 0 24) |     |     |
= 24
Here,fact-iter-accisalwaysinvokedinthesamecontrolcontext: in
fact-iter-acccallsitself,
| thiscase, nocontext | atall. When |     | it doessoat |
| ------------------- | ----------- | --- | ----------- |
the “tailend” of an execution of fact-iter-acc. No promise is made to
do anything with the returned value other than to return it as the result of
the call to fact-iter-acc. We call this a tail call. Thus each step in the
| derivationabovehastheform(fact-iter-acc |     | n a). |     |
| --------------------------------------- | --- | ----- | --- |
Whenaproceduresuchasfactexecutes,additionalcontrolinformation
isrecordedwitheachrecursivecall,andthisinformationisretaineduntilthe
callreturns. Thisreflectsgrowthofthecontrolcontextinthefirstderivation
above.Suchaprocessissaidtoexhibitrecursivecontrolbehavior.
By contrast, no additional control information need be recorded when
fact-iter-acccallsitself. Thisisreflectedinthederivationbyrecursive
callsoccurringatthesamelevelwithintheexpression(ontheoutsideinthe
derivationabove).Insuchcasesthesystemdoesnotneedanever-increasing
amountofmemoryforcontrolcontextsasthedepthofrecursion(thenum-
ber of recursive calls without corresponding returns) increases. A process
that uses a bounded amount of memory for control information is said to
exhibititerativecontrolbehavior.
Whydotheseprogramsexhibitdifferentcontrolbehavior?Intherecursive
definitionoffactorial,theprocedurefactiscalledinanoperandposition. We
needtosavecontextaroundthiscallbecauseweneedtorememberthatafter
the evaluation of the procedure call, we still need to finish evaluating the
operandsandexecutingtheoutercall,inthiscasetothewaitingmultiplica-
tion. Thisleadsustoanimportantprinciple:

5.1 AContinuation-PassingInterpreter 141
Itisevaluationofoperands,notthecallingofprocedures,that
makesthecontrolcontextgrow.
Inthischapterwewilllearnhowtotrackandmanipulatecontrolcontexts.
Ourcentraltoolwillbethedatatypeofcontinuations. Continuations arean
abstraction of the notion of control context, much as environments are an
abstraction of data contexts. We will explore continuations by writing an
interpreter that explicitly passes a continuation parameter, just as our pre-
viousinterpretersexplicitlypassedanenvironmentparameter. Oncewedo
thisforthesimplecases,wecanseehowtoaddtoourlanguagefacilitiesthat
manipulate control contexts in more complicated ways, such as exceptions
andthreads.
Inchapter6weshowhowthesametechniquesweusedtotransformthe
interpreter can be applied to any program. We say that a program trans-
formedin thismanner is incontinuation-passingstyle. Chapter6 alsoshows
severalotherimportantusesofcontinuations.
5.1 AContinuation-Passing Interpreter
Inour newinterpreter,the major proceduressuchasvalue-ofwilltake a
third parameter. This new parameter, the continuation, is intended to be an
abstractionofthecontrolcontextinwhicheachexpressionisevaluated.
Webeginwithaninterpreterinfigure5.1ofthelanguageLETRECofsec-
tion 3.4. We refer to the result of value-of-programas a FinalAnswer to
emphasizethatthisexpressedvalueisthefinalvalueoftheprogram.
Our goal is to rewrite the interpreter so that no call to value-of builds
control context. When the control context needs to grow, we extend the
continuationparameter,muchasweextendedtheenvironmentintheinter-
pretersof chapter 3 as the program builds up data context. By making the
control context explicit, we can see how it grows and shrinks, and later, in
sections5.4–5.5wewilluseittoaddnewcontrolbehaviortoourlanguage.
Now,weknowthatanenvironmentisarepresentationofafunctionfrom
symbols to denoted values. What does a continuation represent? The con-
tinuationofanexpressionrepresentsaprocedurethattakestheresultofthe
expression and completes the computation. So our interface must include
aprocedureapply-contthattakesacontinuationcontandanexpressed
valuevalandfinishesthe computationasspecified bycont. Thecontract
forapply-contwillbe

142 5 Continuation-PassingInterpreters
| FinalAnswer | =   | ExpVal |     |     |     |     |     |     |
| ----------- | --- | ------ | --- | --- | --- | --- | --- | --- |
→
| value-of-program |                  | :                | Program                        | FinalAnswer     |           |              |                 |         |
| ---------------- | ---------------- | ---------------- | ------------------------------ | --------------- | --------- | ------------ | --------------- | ------- |
| (define          | value-of-program |                  |                                |                 |           |              |                 |         |
| (lambda          |                  | (pgm)            |                                |                 |           |              |                 |         |
|                  | (cases           | program          | pgm                            |                 |           |              |                 |         |
|                  | (a-program       |                  | (exp1)                         |                 |           |              |                 |         |
|                  | (value-of        |                  | exp1                           | (init-env)))))) |           |              |                 |         |
| value-of         | : Exp            | × Env            | →                              | ExpVal          |           |              |                 |         |
| (define          | value-of         |                  |                                |                 |           |              |                 |         |
| (lambda          |                  | (exp env)        |                                |                 |           |              |                 |         |
|                  | (cases           | expression       | exp                            |                 |           |              |                 |         |
|                  | (const-exp       |                  | (num)                          | (num-val        | num))     |              |                 |         |
|                  | (var-exp         | (var)            | (apply-env                     |                 | env       | var))        |                 |         |
|                  | (diff-exp        | (exp1            | exp2)                          |                 |           |              |                 |         |
|                  | (let             | ((num1           | (expval->num                   |                 | (value-of |              | exp1            | env)))  |
|                  |                  | (num2            | (expval->num                   |                 | (value-of |              | exp2            | env)))) |
|                  |                  | (num-val         | (- num1                        | num2))))        |           |              |                 |         |
|                  | (zero?-exp       |                  | (exp1)                         |                 |           |              |                 |         |
|                  | (let             | ((num1           | (expval->num                   |                 | (value-of |              | exp1            | env)))) |
|                  |                  | (if (zero?       | num1)                          | (bool-val       |           | #t)          | (bool-val       | #f))))  |
|                  | (if-exp          | (exp1            | exp2                           | exp3)           |           |              |                 |         |
|                  | (if              | (expval->bool    |                                | (value-of       |           | exp1         | env))           |         |
|                  |                  | (value-of        | exp2                           | env)            |           |              |                 |         |
|                  |                  | (value-of        | exp3                           | env)))          |           |              |                 |         |
|                  | (let-exp         | (var             | exp1                           | body)           |           |              |                 |         |
|                  | (let             | ((val1           | (value-of                      |                 | exp1      | env)))       |                 |         |
|                  |                  | (value-of        | body                           | (extend-env     |           | var          | val1 env))))    |         |
|                  | (proc-exp        | (var             | body)                          |                 |           |              |                 |         |
|                  | (proc-val        |                  | (procedure                     |                 | var body  | env)))       |                 |         |
|                  | (call-exp        | (rator           |                                | rand)           |           |              |                 |         |
|                  | (let             | ((proc1          | (expval->proc                  |                 |           | (value-of    | rator           | env)))  |
|                  |                  | (arg             | (value-of                      |                 | rand      | env)))       |                 |         |
|                  |                  | (apply-procedure |                                | proc1           | arg)))    |              |                 |         |
|                  | (letrec-exp      |                  | (p-name                        | b-var           | p-body    | letrec-body) |                 |         |
|                  | (value-of        |                  | letrec-body                    |                 |           |              |                 |         |
|                  |                  | (extend-env-rec  |                                | p-name          | b-var     | p-body       | env))))))       |         |
|                  |                  |                  | ×                              |                 | →         |              |                 |         |
| apply-procedure  |                  | : Proc           |                                | ExpVal          | ExpVal    |              |                 |         |
| (define          | apply-procedure  |                  |                                |                 |           |              |                 |         |
| (lambda          |                  | (proc1           | val)                           |                 |           |              |                 |         |
|                  | (cases           | proc proc1       |                                |                 |           |              |                 |         |
|                  | (procedure       |                  | (var body                      | saved-env)      |           |              |                 |         |
|                  | (value-of        |                  | body                           | (extend-env     |           | var val      | saved-env)))))) |         |
|                  |                  | Figure5.1        | Environment-passingInterpreter |                 |           |              |                 |         |

5.1 AContinuation-PassingInterpreter 143
| FinalAnswer |     | = ExpVal |        |     |             |     |     |
| ----------- | --- | -------- | ------ | --- | ----------- | --- | --- |
|             |     |          | ×      | →   |             |     |     |
| apply-cont  |     | : Cont   | ExpVal |     | FinalAnswer |     |     |
Wecalltheresultofapply-contaFinalAnswertoremindourselvesthatit
isthefinalvalueofthecomputation: itwillnotbeusedbyanyotherpartof
ourprogram.
Whatkind of continuation-builderswillbeincluded intheinterface? We
will discover these continuation-builders as we analyze the interpreter. To
begin, we willneeda continuation-builder forthe contextthatsaysthereis
nothingmoretodowiththevalueofthecomputation. Wecallthiscontinu-
ation(end-cont),andwewillspecifyitby
| (apply-cont |     |     | (end-cont) | val) |     |     |     |
| ----------- | --- | --- | ---------- | ---- | --- | --- | --- |
= (begin
|     | (eopl:printf |     | "End | of  | computation.~%") |     |     |
| --- | ------------ | --- | ---- | --- | ---------------- | --- | --- |
val)
Invoking (end-cont) prints out an end-of-computation message and
returns the value of the program. Because (end-cont) prints out a mes-
sage,wecantellhowmanytimesithasbeeninvoked. Inacorrectcompleted
computation,itshouldbeinvokedexactlyonce.
Werewritevalue-of-programas:
→
| value-of-program                |            |                  | : Program | FinalAnswer |     |                             |     |
| ------------------------------- | ---------- | ---------------- | --------- | ----------- | --- | --------------------------- | --- |
| (define                         |            | value-of-program |           |             |     |                             |     |
| (lambda                         |            | (pgm)            |           |             |     |                             |     |
|                                 | (cases     | program          | pgm       |             |     |                             |     |
|                                 | (a-program |                  | (exp1)    |             |     |                             |     |
|                                 |            | (value-of/k      | exp1      | (init-env)  |     | (end-cont))))))             |     |
| Wecannowbegintowritevalue-of/k. |            |                  |           |             |     | Weconsidereachofthealterna- |     |
tivesinvalue-ofinturn. Thefirstfewlinesofvalue-ofsimplycalculate
avalueandreturnit,withoutcallingvalue-ofagain. Inthecontinuation-
passinginterpreter,thesesamelinessendthesamevaluetothecontinuation
bycallingapply-cont:
|            |            |             | ×          | ×           | →           |                 |            |
| ---------- | ---------- | ----------- | ---------- | ----------- | ----------- | --------------- | ---------- |
| value-of/k |            | : Exp       | Env        | Cont        | FinalAnswer |                 |            |
| (define    |            | value-of/k  |            |             |             |                 |            |
| (lambda    |            | (exp        | env cont)  |             |             |                 |            |
|            | (cases     | expression  |            | exp         |             |                 |            |
|            | (const-exp |             | (num)      | (apply-cont |             | cont (num-val   | num)))     |
|            | (var-exp   |             | (var)      | (apply-cont |             | cont (apply-env | env var))) |
|            | (proc-exp  |             | (var       | body)       |             |                 |            |
|            |            | (apply-cont | cont       |             |             |                 |            |
|            |            | (proc-val   | (procedure |             | var         | body env))))    |            |
...)))

144 5 Continuation-PassingInterpreters
Uptonowtheonlypossiblevalueofconthasbeentheendcontinuation,
but that will change momentarily. It is easy to check that if the program
consists of an expression of one of these forms, the value of the expression
willbesuppliedtoend-cont(throughapply-cont).
Thebehaviorofletrecisalmostassimple: itcreatesanewenvironment
value-of,
| without | calling |     | and | then | evaluates | the body | in the new envi- |
| ------- | ------- | --- | --- | ---- | --------- | -------- | ---------------- |
ronment. Thevalueofthebodybecomesthevalueoftheentireexpression.
That means that the body is performed in the same control context as the
entireexpression. Thereforethevalueofthebodyshouldbereturnedtothe
| continuationoftheentireexpression. |             |                 |             |        | Thereforewewrite |              |      |
| ---------------------------------- | ----------- | --------------- | ----------- | ------ | ---------------- | ------------ | ---- |
|                                    | (letrec-exp |                 | (p-name     | b-var  | p-body           | letrec-body) |      |
|                                    | (value-of/k |                 | letrec-body |        |                  |              |      |
|                                    |             | (extend-env-rec |             | p-name | b-var            | p-body       | env) |
cont))
Thisillustratesageneralprinciple:
TailCallsDon’tGrowtheContinuation
If the value of exp is returned as the value of exp , then exp and exp
|     |     | 1   |     |     |     | 2   | 1 2 |
| --- | --- | --- | --- | --- | --- | --- | --- |
shouldruninthesamecontinuation.
Itwouldnotbecorrecttowrite
|     | (letrec-exp |                 | (p-name     | b-var  | p-body | letrec-body) |      |
| --- | ----------- | --------------- | ----------- | ------ | ------ | ------------ | ---- |
|     | (apply-cont |                 | cont        |        |        |              |      |
|     |             | (value-of/k     | letrec-body |        |        |              |      |
|     |             | (extend-env-rec |             | p-name |        | b-var p-body | env) |
(end-cont))))
becausethe callto value-of/kisinanoperandposition: itappearsasan
| operandtoapply-cont. |     |     |     |     |     |     | (end-cont) |
| -------------------- | --- | --- | --- | --- | --- | --- | ---------- |
Inaddition,usingthecontinuation
causestheend-of-computationmessagetobeprintedbeforethecomputation
isfinished,soanerrorlikethisiseasytodetect.
Let us next consider a zero? expression. In a zero? expression, we
wanttoevaluatethe argument, andthenreturnavaluetothe continuation
| dependingonthevalueoftheargument. |     |     |     |     | Soweevaluatetheargumentina |     |     |
| --------------------------------- | --- | --- | --- | --- | -------------------------- | --- | --- |
newcontinuationthatwilllookatthereturnedvalueanddotherightthing.

| 5.1 AContinuation-PassingInterpreter |     |     |     |     |     |     | 145 |
| ------------------------------------ | --- | --- | --- | --- | --- | --- | --- |
Soinvalue-of/kwewrite
|                  | (zero?-exp  |             | (exp1)                                  |         |            |     |     |
| ---------------- | ----------- | ----------- | --------------------------------------- | ------- | ---------- | --- | --- |
|                  | (value-of/k |             | exp1                                    | env     |            |     |     |
|                  |             | (zero1-cont |                                         | cont))) |            |     |     |
| where(zero1-cont |             |             | cont)isacontinuationwiththepropertythat |         |            |     |     |
| (apply-cont      |             | (zero1-cont |                                         |         | cont) val) |     |     |
| = (apply-cont    |             |             | cont                                    |         |            |     |     |
(bool-val
|     | (zero? | (expval->num |     |     | val)))) |     |     |
| --- | ------ | ------------ | --- | --- | ------- | --- | --- |
Justaswithletrec,wecouldnotwriteinvalue-of/k
|     | (zero?-exp |             | (exp1)      |      |      |                   |     |
| --- | ---------- | ----------- | ----------- | ---- | ---- | ----------------- | --- |
|     | (let       | ((val       | (value-of/k |      | exp1 | env (end-cont)))) |     |
|     |            | (apply-cont |             | cont |      |                   |     |
(bool-val
|     |     | (zero? |     | (expval->num |     | val)))))) |     |
| --- | --- | ------ | --- | ------------ | --- | --------- | --- |
because the call to value-of/k is in operand position. The right-hand
side of a let is in operand position, because (let ((var exp )) exp )
1 2
is equivalent to ((lambda (var) exp ) exp ). The value of the call to
|     |     |     |     |     | 2   | 1   |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
value-of/keventuallybecomestheoperandofexpval->num.Asbefore,
if we ran this code, the end-of-computation message would appear twice:
onceinthemiddleofthecomputationandonceattherealend.
Aletexpressionisjust slightly morecomplicated thanazero?expres-
sion: afterevaluatingtheright-handside,weevaluatethebodyinasuitably
| extendedenvironment. |          |             | Theoriginalcodeforletwas |       |              |        |     |
| -------------------- | -------- | ----------- | ------------------------ | ----- | ------------ | ------ | --- |
|                      | (let-exp | (var        | exp1                     | body) |              |        |     |
|                      | (let     | ((val1      | (value-of                |       | exp1         | env))) |     |
|                      |          | (value-of   | body                     |       |              |        |     |
|                      |          | (extend-env |                          | var   | val1 env)))) |        |     |
toevaluateexp1in
| Inthe continuation-passing        |             |               |      | interpreter,we |                       | need    | acon- |
| --------------------------------- | ----------- | ------------- | ---- | -------------- | --------------------- | ------- | ----- |
| textthatwillfinishthecomputation. |             |               |      |                | Soinvalue-of/kwewrite |         |       |
|                                   | (let-exp    | (var          | exp1 | body)          |                       |         |       |
|                                   | (value-of/k |               | exp1 | env            |                       |         |       |
|                                   |             | (let-exp-cont |      | var            | body env              | cont))) |       |
andweaddtoourcontinuationsinterfacethespecification

| 146           |               |                  | 5 Continuation-PassingInterpreters |            |     |
| ------------- | ------------- | ---------------- | ---------------------------------- | ---------- | --- |
| (apply-cont   | (let-exp-cont |                  | var body env                       | cont) val) |     |
| = (value-of/k |               | body (extend-env | var val                            | env) cont) |     |
Thevalueofthebodyoftheletexpressionbecomesthevalueofthelet
expression,sothebodyoftheletexpressionisevaluatedinthesamecon-
tinuation as the entire let expression. This is another instance of the Tail
CallsDon’tGrowtheContinuationprinciple.
Let us move on to if expressions. In an if expression, the first thing
evaluated is the test, but the result of the test is not the value of the entire
expression. Weneedtobuildanewcontinuationthatwillseeiftheresultof
thetestexpressionisatruevalue,andevaluateeitherthetrueexpressionor
Soinvalue-of/kwewrite
thefalseexpression.
| (if-exp |               | (exp1 exp2 exp3) |          |         |     |
| ------- | ------------- | ---------------- | -------- | ------- | --- |
|         | (value-of/k   | exp1 env         |          |         |     |
|         | (if-test-cont | exp2             | exp3 env | cont))) |     |
if-test-cont
| where |     | is a new continuation-builder |     | subject to | the specifi- |
| ----- | --- | ----------------------------- | --- | ---------- | ------------ |
cation
| (apply-cont | (if-test-cont |               | exp2 exp3 | env cont) val) |     |
| ----------- | ------------- | ------------- | --------- | -------------- | --- |
| = (if       | (expval->bool | val)          |           |                |     |
| (value-of/k |               | exp2 envcont) |           |                |     |
| (value-of/k |               |               | cont))    |                |     |
exp3 env
Sofar,wehavefourcontinuation-builders. Wecanimplementthemusing
either a procedural representation or a data structure representation. The
proceduralrepresentationisin figure5.2and the datastructure representa-
tion,usingdefine-datatype,isinfigure5.3.
Hereisasample calculationthatshows howthese piecesfittogether. As
wedidinsection3.3,wewrite«exp»todenotetheabstractsyntaxtreeasso-
ρ
ciated with the expression exp. Assume 0 is an environment in which b
is bound to (bool-val #t) and assume cont is the initial continuation,
0
whichisthevalueof(end-cont). Thecommentaryisinformalandshould
value-of/k
be checked against the definition of and the specification of
apply-cont. Thisexampleiscontrivedbecausewehaveletrectointro-
duceproceduresbutwedonotyethaveawaytoinvokethem.

5.1 AContinuation-PassingInterpreter 147
| Cont = | ExpVal → | FinalAnswer |     |     |
| ------ | -------- | ----------- | --- | --- |
→
| end-cont | : ()     | Cont |     |     |
| -------- | -------- | ---- | --- | --- |
| (define  | end-cont |      |     |     |
| (lambda  | ()       |      |     |     |
| (lambda  | (val)    |      |     |     |
(begin
|     | (eopl:printf | "End of computation.~%") |     |     |
| --- | ------------ | ------------------------ | --- | --- |
val))))
→
| zero1-cont  | : Cont     | Cont |     |     |
| ----------- | ---------- | ---- | --- | --- |
| (define     | zero1-cont |      |     |     |
| (lambda     | (cont)     |      |     |     |
| (lambda     | (val)      |      |     |     |
| (apply-cont |            | cont |     |     |
(bool-val
|              | (zero?        | (expval->num val)))))))                 |              |          |
| ------------ | ------------- | --------------------------------------- | ------------ | -------- |
| let-exp-cont | : Var         | × Exp × Env ×                           | Cont → Cont  |          |
| (define      | let-exp-cont  |                                         |              |          |
| (lambda      | (var          | body env cont)                          |              |          |
| (lambda      | (val)         |                                         |              |          |
| (value-of/k  |               | body (extend-env                        | var val env) | cont)))) |
| if-test-cont | : Exp         | × Exp × Env × Cont                      | → Cont       |          |
| (define      | if-test-cont  |                                         |              |          |
| (lambda      | (exp2         | exp3 env cont)                          |              |          |
| (lambda      | (val)         |                                         |              |          |
| (if          | (expval->bool | val)                                    |              |          |
|              | (value-of/k   | exp2 env cont)                          |              |          |
|              | (value-of/k   | exp3 env cont)))))                      |              |          |
| apply-cont   | : Cont        | × ExpVal → FinalAnswer                  |              |          |
| (define      | apply-cont    |                                         |              |          |
| (lambda      | (cont         | v)                                      |              |          |
| (cont        | v)))          |                                         |              |          |
|              | Figure5.2     | Proceduralrepresentationofcontinuations |              |          |

| 148              |     |              |               | 5 Continuation-PassingInterpreters |
| ---------------- | --- | ------------ | ------------- | ---------------------------------- |
| (define-datatype |     | continuation | continuation? |                                    |
(end-cont)
(zero1-cont
|     | (cont continuation?)) |     |     |     |
| --- | --------------------- | --- | --- | --- |
(let-exp-cont
|     | (var identifier?)     |     |     |     |
| --- | --------------------- | --- | --- | --- |
|     | (body expression?)    |     |     |     |
|     | (env environment?)    |     |     |     |
|     | (cont continuation?)) |     |     |     |
(if-test-cont
|            | (exp2 expression?)     |        |             |     |
| ---------- | ---------------------- | ------ | ----------- | --- |
|            | (exp3 expression?)     |        |             |     |
|            | (env environment?)     |        |             |     |
|            | (cont continuation?))) |        |             |     |
|            |                        | ×      | →           |     |
| apply-cont | : Cont                 | ExpVal | FinalAnswer |     |
| (define    | apply-cont             |        |             |     |
| (lambda    | (cont                  | val)   |             |     |
|            | (cases continuation    |        | cont        |     |
|            | (end-cont              | ()     |             |     |
(begin
|     | (eopl:printf | "End | of computation.~%") |     |
| --- | ------------ | ---- | ------------------- | --- |
val))
|     | (zero1-cont | (saved-cont) |     |     |
| --- | ----------- | ------------ | --- | --- |
|     | (apply-cont | saved-cont   |     |     |
(bool-val
|     | (zero?            | (expval->num                               | val)))))       |                  |
| --- | ----------------- | ------------------------------------------ | -------------- | ---------------- |
|     | (let-exp-cont     | (var                                       | body saved-env | saved-cont)      |
|     | (value-of/k       | body                                       |                |                  |
|     | (extend-env       | var                                        | val saved-env) | saved-cont))     |
|     | (if-test-cont     | (exp2                                      | exp3 saved-env | saved-cont)      |
|     | (if (expval->bool |                                            | val)           |                  |
|     | (value-of/k       | exp2                                       | saved-env      | saved-cont)      |
|     | (value-of/k       | exp3                                       | saved-env      | saved-cont)))))) |
|     | Figure5.3         | Datastructurerepresentationofcontinuations |                |                  |

5.1 AContinuation-PassingInterpreter 149
| (value-of/k |                   | <<letrec |        | p(x) = | x in | if b then | 3 else 4>> |
| ----------- | ----------------- | -------- | ------ | ------ | ---- | --------- | ---------- |
| ρ cont      | )                 |          |        |        |      |           |            |
| 0           | 0                 |          |        |        |      |           |            |
|             | ρ                 |          |        |        | ρ    |           |            |
| = letting   | be(extend-env-rec |          |        |        | ...  | )         |            |
|             | 1                 |          |        |        |      | 0         |            |
| (value-of/k |                   | <<if     | b then | 3 else | 4>>  | ρ cont    | )          |
|             |                   |          |        |        |      | 1         | 0          |
= next,evaluatethetestexpression
| (value-of/k |     | <<b>> | ρ (test-cont |     | <<3>> | <<4>> | ρ ))     |
| ----------- | --- | ----- | ------------ | --- | ----- | ----- | -------- |
|             |     |       | 1            |     |       |       | 1 cont 0 |
= sendthevalueofbtothecontinuation
ρ
| (apply-cont |     | (test-cont |     | <<3>> | <<4>> | cont | )   |
| ----------- | --- | ---------- | --- | ----- | ----- | ---- | --- |
|             |     |            |     |       |       | 1    | 0   |
|             |     | (bool-val  |     | #t))  |       |      |     |
= evaluatethethen-expression
| (value-of/k |     | <<3>> | ρ      | )   |     |     |     |
| ----------- | --- | ----- | ------ | --- | --- | --- | --- |
|             |     |       | 1 cont | 0   |     |     |     |
= sendthevalueoftheexpressiontothecontinuation
| (apply-cont |     | cont | (num-val | 3)) |     |     |     |
| ----------- | --- | ---- | -------- | --- | --- | --- | --- |
0
= invokethefinalcontinuationwiththefinalanswer
| (begin | (eopl:printf |     | ...) | (num-val |     | 3)) |     |
| ------ | ------------ | --- | ---- | -------- | --- | --- | --- |
Differenceexpressionsaddanewwrinkletoourinterpreterbecausethey
must evaluate both operands. We begin as we did with if, evaluating the
firstargument:
| (diff-exp       |             | (exp1 | exp2) |                                       |         |     |     |
| --------------- | ----------- | ----- | ----- | ------------------------------------- | ------- | --- | --- |
|                 | (value-of/k |       | exp1  | env                                   |         |     |     |
|                 | (diff1-cont |       | exp2  | env                                   | cont))) |     |     |
| When(diff1-cont |             | exp2  |       | env cont)receivesavalue,itshouldeval- |         |     |     |
uate exp2 in a context that saves the value of exp1. We specify this by
writing
| (apply-cont      |     | (diff1-cont |        | exp2                   | env cont) | val1) |     |
| ---------------- | --- | ----------- | ------ | ---------------------- | --------- | ----- | --- |
| = (value-of/k    |     | exp2        | env    |                        |           |       |     |
| (diff2-cont      |     | val1        | cont)) |                        |           |       |     |
| Whena(diff2-cont |     |             | val1   | cont)receivesavalue,we |           |       |     |
knowtheval-
ues of both operands so we can proceed to send their difference to cont,
| whichhasbeenwaitingtoreceiveit. |          |              |      | Thespecificationis |         |       |     |
| ------------------------------- | -------- | ------------ | ---- | ------------------ | ------- | ----- | --- |
| (apply-cont                     |          | (diff2-cont  |      | val1               | cont)   | val2) |     |
| = (let                          | ((num1   | (expval->num |      |                    | val1))  |       |     |
|                                 | (num2    | (expval->num |      |                    | val2))) |       |     |
| (apply-cont                     |          | cont         |      |                    |         |       |     |
|                                 | (num-val | (-           | num1 | num2))))           |         |       |     |

150 5 Continuation-PassingInterpreters
Let’swatchthissystemdoanexample.
(value-of/k
<<-(-(44,11),3)>>
ρ
0
#(struct:end-cont))
= startworkingonfirstoperand
(value-of/k
<<-(44,11)>>
ρ
0
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont)))
= startworkingonfirstoperand
(value-of/k
<<44>>
ρ
0
#(struct:diff1-cont <<11>> ρ
0
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont))))
= sendvalueof<<44>>tocontinuation
(apply-cont
#(struct:diff1-cont <<11>> ρ
0
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont)))
(num-val 44))
= nowstartworkingonsecondoperand
(value-of/k
<<11>>
ρ
0
#(struct:diff2-cont (num-val 44)
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont))))
= sendvaluetocontinuation
(apply-cont
#(struct:diff2-cont (num-val 44)
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont)))
(num-val 11))
= 44−11is33,sendthattothecontinuation
(apply-cont
#(struct:diff1-cont <<3>> ρ
0
#(struct:end-cont))
(num-val 33))

5.1 AContinuation-PassingInterpreter 151
= startworkingonsecondoperand<<3>>
(value-of/k
<<3>>
ρ
0
| #(struct:diff2-cont |     | (num-val 33) |     |     |
| ------------------- | --- | ------------ | --- | --- |
#(struct:end-cont)))
= sendvaluetocontinuation
(apply-cont
| #(struct:diff2-cont |     | (num-val 33) |     |     |
| ------------------- | --- | ------------ | --- | --- |
#(struct:end-cont))
| (num-val | 3)) |     |     |     |
| -------- | --- | --- | --- | --- |
= 33−3is30,sendthattothecontinuation
(apply-cont
#(struct:end-cont)
| (num-val                                    | 30)) |                                      |     |                 |
| ------------------------------------------- | ---- | ------------------------------------ | --- | --------------- |
| apply-contprintsoutthecompletionmessage"End |      |                                      |     | of computation" |
| andreturns(num-val                          |      | 30)asthefinalanswerofthecomputation. |     |                 |
Thelastthinginourlanguageisprocedureapplication.Intheenvironment-
passinginterpreter,wewrote
|     | (call-exp (rator | rand)          |           |              |
| --- | ---------------- | -------------- | --------- | ------------ |
|     | (let ((proc1     | (expval->proc  | (value-of | rator env))) |
|     | (val             | (value-of rand | env)))    |              |
|     | (apply-procedure | proc1          | val)))    |              |
Here we have two calls to consider, as we did in diff-exp. So we must
choose one of them to be first, and then we must transform the remain-
der to handle the second. Furthermore, we will have to pass the continu-
ationtoapply-procedure,becauseapply-procedurecontainsacallto
value-of/k.
Wechoosetheevaluationoftheoperatortobefirst,soinvalue-of/kwe
write
|     | (call-exp (rator | rand)            |     |     |
| --- | ---------------- | ---------------- | --- | --- |
|     | (value-of/k      | rator env        |     |     |
|     | (rator-cont      | rand env cont))) |     |     |
Aswith diff-exp, a rator-contwill evaluatethe operand in a suitable
continuation:
| (apply-cont | (rator-cont | rand | env cont) val1) |     |
| ----------- | ----------- | ---- | --------------- | --- |
= (value-of/k
rand env
|     | (rand-cont val1 | cont)) |     |     |
| --- | --------------- | ------ | --- | --- |

152 5 Continuation-PassingInterpreters
Whenarand-contreceivesavalue,itisreadytocalltheprocedure:
| (apply-cont |                    |         | (rand-cont |               | val1  | cont) | val2)       |     |
| ----------- | ------------------ | ------- | ---------- | ------------- | ----- | ----- | ----------- | --- |
| =           | (let               | ((proc1 |            | (expval->proc |       |       | val1)))     |     |
|             | (apply-procedure/k |         |            |               | proc1 |       | val2 cont)) |     |
Last,wemustmodifyapply-proceduretofitinthiscontinuation-passing
style:
|                   |            |                   |       | ×         |        | ×          | →                |     |
| ----------------- | ---------- | ----------------- | ----- | --------- | ------ | ---------- | ---------------- | --- |
| apply-procedure/k |            |                   | :     | Proc      | ExpVal |            | Cont FinalAnswer |     |
| (define           |            | apply-procedure/k |       |           |        |            |                  |     |
|                   | (lambda    | (proc1            |       | val cont) |        |            |                  |     |
|                   | (cases     | proc              | proc1 |           |        |            |                  |     |
|                   | (procedure |                   |       | (var body |        | saved-env) |                  |     |
|                   |            | (value-of/k       |       | body      |        |            |                  |     |
|                   |            | (extend-env       |       | var       | val    | saved-env) |                  |     |
cont)))))
This completes the presentation of the continuation-passing interpreter.
Thecompleteinterpreterisshowninfigures5.4and5.5. Thecompletespec-
ificationofthecontinuationsisshowninfigure5.6.
Nowwecanchecktheassertionthatitisevaluationofactualparameters,
not the calling of procedures, that requires growing the control context. In
particular, if we evaluate a procedure call (exp exp ) in some continua-
|     |     |     |     |     |     |     | 1 2 |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
tion cont , the body of the procedure to which exp evaluates will also be
|                                | 1   |     |     |     |     |     | 1   |     |
| ------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- |
| evaluatedinthecontinuationcont |     |     |     |     | .   |     |     |     |
1
Butprocedurecallsdonotthemselvesgrowcontrolcontexts. Considerthe
evaluationof(exp exp ),wherethevalueofexp issomeprocedure proc
|                  |     |        | 1                       | 2     |     |        | 1   | 1   |
| ---------------- | --- | ------ | ----------------------- | ----- | --- | ------ | --- | --- |
| andthevalueofexp |     |        | issomeexpressedvalueval |       |     |        | .   |     |
|                  |     |        | 2                       |       |     |        | 2   |     |
| (value-of/k      |     | <<(exp |                         | )>>   | ρ   | )      |     |     |
|                  |     |        | 1                       | exp 2 | 1   | cont 1 |     |     |
= evaluateoperator
ρ
| (value-of/k |     | <<exp | >>    |      |         |     |     |     |
| ----------- | --- | ----- | ----- | ---- | ------- | --- | --- | --- |
|             |     |       | 1     | 1    |         |     |     |     |
| (rator-cont |     |       | <<exp | >> ρ | cont )) |     |     |     |
|             |     |       | 2     | 1    | 1       |     |     |     |
= sendtheproceduretothecontinuation
(apply-cont
| (rator-cont |     |     | <<exp | >> ρ | )      |     |     |     |
| ----------- | --- | --- | ----- | ---- | ------ | --- | --- | --- |
|             |     |     | 2     | 1    | cont 1 |     |     |     |
proc 1 )
= evaluatetheoperand
ρ
| (value-of/k |     | <<exp | >>   |     |     |     |     |     |
| ----------- | --- | ----- | ---- | --- | --- | --- | --- | --- |
|             |     |       | 2    | 1   |     |     |     |     |
| (rand-cont  |     | proc  | cont | ))  |     |     |     |     |
|             |     |       | 1    | 1   |     |     |     |     |
= sendtheargumenttothecontinuation
(apply-cont
| (rand-cont |     | proc | 1 cont | 1 ) |     |     |     |     |
| ---------- | --- | ---- | ------ | --- | --- | --- | --- | --- |
val )
2
= applytheprocedure
| (apply-procedure/k |     |     |     | proc val | cont | )   |     |     |
| ------------------ | --- | --- | --- | -------- | ---- | --- | --- | --- |
|                    |     |     |     | 1        | 2    | 1   |     |     |

5.1 AContinuation-PassingInterpreter 153
Sotheprocedureisapplied,anditsbodyisevaluated,inthesamecontin-
uationinwhichitwascalled. Itistheevaluationofoperands,nottheentry
intoaprocedurebody,thatrequirescontrolcontext.
(cid:3)
Exercise5.1 [ ]Implementthisdatatypeofcontinuationsusingtheproceduralrep-
resentation.
(cid:3)
Exercise5.2 [ ]Implementthisdatatypeofcontinuationsusingadata-structurerep-
resentation.
(cid:3)
Exercise5.3 [ ] Addlet2tothisinterpreter.Alet2expressionislikealetexpres-
sion,exceptthatitdefinesexactlytwovariables.
(cid:3)
Exercise5.4 [ ] Addlet3tothisinterpreter.Alet3expressionislikealetexpres-
sion,exceptthatitdefinesexactlythreevariables.
(cid:3)
Exercise5.5 [ ]Addliststothelanguage,asinexercise3.9.
(cid:3)(cid:3)
Exercise5.6 [ ] Add a list expression to the language, as in exercise 3.10. As
ahint, consideradding two new continuation-builders, one forevaluating the first
elementofthelistandoneforevaluatingtherestofthelist.
(cid:3)(cid:3)
Exercise5.7 [ ] Addmultideclarationlet(exercise3.16)tothisinterpreter.
(cid:3)(cid:3)
Exercise5.8 [ ] Addmultiargumentprocedures(exercise3.21)tothisinterpreter.
(cid:3)(cid:3)
Exercise5.9 [ ] Modify this interpreter to implement the IMPLICIT-REFS lan-
guage. As ahint, considerincluding anew continuation-builder (set-rhs-cont
env var cont).
(cid:3)(cid:3)
Exercise5.10 [ ] Modifythesolutiontothepreviousexercisesothattheenviron-
mentisnotkeptinthecontinuation.
(cid:3)(cid:3)
Exercise5.11 [ ] Add the begin expression of exercise 4.4 to the continuation-
passinginterpreter. Besurethat no callto value-oforvalue-of-randsoccurs
inapositionthatwouldbuildcontrolcontext.
(cid:3)
Exercise5.12 [ ] Instrumenttheinterpreteroffigures5.4–5.6toproduceoutputsim-
ilartothatofthecalculationonpage150.
(cid:3)
Exercise5.13 [ ] Translatethedefinitionsoffactandfact-iterintotheLETREC
language. You may add a multiplication operator to the language. Then, using
the instrumented interpreter of the previous exercise, compute (fact 4) and
(fact-iter 4). Comparethem tothe calculations atthe beginningofthischap-
ter. Find(* 4 (* 3 (* 2 (fact 1))))inthetraceof(fact 4). Whatisthe
continuationofapply-procedure/kforthiscallof(fact 1)?
(cid:3)
Exercise5.14 [ ] The instrumentation of the preceding exercise produces volu-
minous output. Modify the instrumentation to track instead only the size of the
largestcontinuation used during the calculation. We measure the size of a contin-
uationbythe numberofcontinuation-buildersemployedinitsconstruction, sothe

154 5 Continuation-PassingInterpreters
| value-of-program |                  | :          | Program     | → FinalAnswer |             |                 |            |
| ---------------- | ---------------- | ---------- | ----------- | ------------- | ----------- | --------------- | ---------- |
| (define          | value-of-program |            |             |               |             |                 |            |
| (lambda          | (pgm)            |            |             |               |             |                 |            |
|                  | (cases           | program    | pgm         |               |             |                 |            |
|                  | (a-program       |            | (exp1)      |               |             |                 |            |
|                  | (value-of/k      |            | exp1        | (init-env)    |             | (end-cont)))))) |            |
| value-of/k       | :                | ×          | ×           |               | →           |                 |            |
|                  |                  | Exp        | Env         | Cont          | FinalAnswer |                 |            |
| (define          | value-of/k       |            |             |               |             |                 |            |
| (lambda          | (exp             | env        | cont)       |               |             |                 |            |
|                  | (cases           | expression | exp         |               |             |                 |            |
|                  | (const-exp       |            | (num)       | (apply-cont   |             | cont (num-val   | num)))     |
|                  | (var-exp         | (var)      | (apply-cont |               | cont        | (apply-env      | env var))) |
|                  | (proc-exp        | (var       | body)       |               |             |                 |            |
|                  | (apply-cont      |            | cont        |               |             |                 |            |
(proc-val
|     |                 | (procedure |             | var body | env)))) |              |      |
| --- | --------------- | ---------- | ----------- | -------- | ------- | ------------ | ---- |
|     | (letrec-exp     |            | (p-name     | b-var    | p-body  | letrec-body) |      |
|     | (value-of/k     |            | letrec-body |          |         |              |      |
|     | (extend-env-rec |            |             | p-name   | b-var   | p-body       | env) |
cont))
|     | (zero?-exp    |        | (exp1)  |          |            |             |     |
| --- | ------------- | ------ | ------- | -------- | ---------- | ----------- | --- |
|     | (value-of/k   |        | exp1    | env      |            |             |     |
|     | (zero1-cont   |        | cont))) |          |            |             |     |
|     | (if-exp       | (exp1  | exp2    | exp3)    |            |             |     |
|     | (value-of/k   |        | exp1    | env      |            |             |     |
|     | (if-test-cont |        |         | exp2     | exp3       | env cont))) |     |
|     | (let-exp      | (var   | exp1    | body)    |            |             |     |
|     | (value-of/k   |        | exp1    | env      |            |             |     |
|     | (let-exp-cont |        |         | var body | env        | cont)))     |     |
|     | (diff-exp     | (exp1  | exp2)   |          |            |             |     |
|     | (value-of/k   |        | exp1    | env      |            |             |     |
|     | (diff1-cont   |        | exp2    | env      | cont)))    |             |     |
|     | (call-exp     | (rator |         | rand)    |            |             |     |
|     | (value-of/k   |        | rator   | env      |            |             |     |
|     | (rator-cont   |        | rand    | env      | cont)))))) |             |     |
Figure5.4
Continuation-passinginterpreter(part1)

5.2 ATrampolinedInterpreter 155
apply-procedure/k : Proc × ExpVal × Cont → FinalAnswer
(define apply-procedure/k
(lambda (proc1 val cont)
(cases proc proc1
(procedure (var body saved-env)
(value-of/k body
(extend-env var val saved-env)
cont)))))
Figure5.5 Continuation-passinginterpreter(part2)
sizeofthelargestcontinuationinthecalculationonpage150is3. Thencalculatethe
valuesoffactandfact-iterappliedtoseveraloperands.Confirmthatthesizeof
thelargestcontinuationusedbyfactgrowslinearlywithitsargument,butthesize
ofthelargestcontinuationusedbyfact-iterisaconstant.
(cid:3)
Exercise5.15 [ ] Our continuation data type contains just the single constant,
end-cont,andalltheothercontinuation-buildershaveasinglecontinuationargu-
ment.Implementcontinuationsbyrepresentingthemaslists,where(end-cont)is
representedbytheemptylist,andeachothercontinuationisrepresentedbyanon-
emptylistwhose car contains adistinctive data structure (calledframe oractivation
record)andwhosecdrcontains theembeddedcontinuation. Observethattheinter-
pretertreatstheselistslikeastack(offrames).
(cid:3)(cid:3)
Exercise5.16 [ ] Extend the continuation-passing interpreter to the language of
exercise4.22. Passacontinuation argumentto result-of, and makesurethat no
calltoresult-ofoccursinapositionthatgrowsacontrolcontext.Sinceastatement
doesnotreturna value, distinguishbetween ordinarycontinuations and continua-
tionsforstatements;thelatterareusuallycalledcommandcontinuations.Theinterface
shouldincludeaprocedureapply-command-contthattakesacommandcontinu-
ationandinvokesit.Implementcommandcontinuationsbothasdatastructuresand
aszero-argumentprocedures.
5.2 ATrampolined Interpreter
One might now be tempted to transcribe the interpreter into an ordinary
procedurallanguage,usingadatastructurerepresentationofcontinuations
toavoidthe needforhigher-orderprocedures. Mostprocedurallanguages,
however, make itdifficulttodothis translation: instead ofgrowing control
contextonlywhennecessary,theyaddtothecontrolcontext(thestack!) on
every procedure call. Since the procedure calls in our system never return
until the veryend of the computation, the stackin these systems continues
togrowuntilthattime.

156 5 Continuation-PassingInterpreters
| (apply-cont | (end-cont) | val) |     |     |     |     |     |
| ----------- | ---------- | ---- | --- | --- | --- | --- | --- |
= (begin
(eopl:printf
| "End | of computation.~%") |     |     |     |     |     |     |
| ---- | ------------------- | --- | --- | --- | --- | --- | --- |
val)
| (apply-cont   | (diff1-cont | exp2        | env   | cont) | val1)       |     |     |
| ------------- | ----------- | ----------- | ----- | ----- | ----------- | --- | --- |
| = (value-of/k | exp2 env    | (diff2-cont |       |       | val1 cont)) |     |     |
| (apply-cont   | (diff2-cont |             | cont) | val2) |             |     |     |
val1
| = (let ((num1 | (expval->num |          | val1))  |       |          |     |     |
| ------------- | ------------ | -------- | ------- | ----- | -------- | --- | --- |
| (num2         | (expval->num |          | val2))) |       |          |     |     |
| (apply-cont   | cont         | (num-val | (-      | num1  | num2)))) |     |     |
| (apply-cont   | (rator-cont  |          |         | cont) | val1)    |     |     |
rand env
| = (value-of/k       | rand env         | (rand-cont |         | val1     | cont))       |       |         |
| ------------------- | ---------------- | ---------- | ------- | -------- | ------------ | ----- | ------- |
| (apply-cont         | (rand-cont       | val1       | cont)   | val2)    |              |       |         |
| = (let ((proc1      | (expval->proc    |            | val1))) |          |              |       |         |
| (apply-procedure/k  |                  | proc1      | val2    | cont))   |              |       |         |
| (apply-cont         | (zero1-cont      | cont)      | val)    |          |              |       |         |
| = (apply-cont       | cont (bool-val   |            | (zero?  |          | (expval->num |       | val)))) |
| (apply-cont         | (if-test-cont    |            | exp2    | exp3     | env cont)    | val)  |         |
| = (if (expval->bool |                  | val)       |         |          |              |       |         |
| (value-of/k         |                  | cont)      |         |          |              |       |         |
|                     | exp2             | env        |         |          |              |       |         |
| (value-of/k         | exp3             | env cont)) |         |          |              |       |         |
| (apply-cont         | (let-exp-cont    |            | var     | body env | cont)        | val1) |         |
| = (value-of/k       | body (extend-env |            |         | var val1 | env)         | cont) |         |
Figure5.6 Specificationofcontinuationsforfigure5.4

5.2 ATrampolinedInterpreter 157
This behavior is not entirely irrational: in such languages almost every
procedurecalloccursontheright-handsideofanassignmentstatement,so
thatalmosteveryprocedurecallmustgrowthecontrolcontexttokeeptrack
ofthependingassignment. Hencethearchitectureisoptimizedforthismost
commoncase. Furthermore,mostlanguagesstoreenvironmentinformation
on the stack, so every procedure call must generate a control context that
rememberstoremovetheenvironmentinformationfromthestack.
In such languages, one solution is to use a technique called trampolining.
Toavoidhavinganunboundedchainofprocedurecalls,webreakthechain
by having one of the procedures in the interpreter actually return a zero-
argumentprocedure.Thisprocedure,whencalled,willcontinuethecompu-
tation. The entire computation is driven by a procedurecalled a trampoline
thatbouncesfromoneprocedurecalltothenext. Forexample,wecaninsert
a (lambda () ...) around the body of apply-procedure/k, since in
ourlanguagenoexpressionwouldrunmorethanaboundedamountoftime
withoutperformingaprocedurecall.
Theresultingcodeisshowninfigure5.7,whichalsoshowsallthetailcalls
intheinterpreter. Sincewe havemodified apply-procedure/ktoreturn
a procedure, rather than an ExpVal, we must rewrite its contract and also
thecontractsofalltheproceduresthatcallit. Wemustthereforereviewthe
contractsofalltheproceduresintheinterpreter.
We begin with value-of-program. Since this is the procedure that
is used to invoke the interpreter, its contract is unchanged. It calls
value-of/k and passes the result to trampoline. Since we are now
doing something with the result of value-of/k, that result is something
other than a FinalAnswer. How canthat be, since we have not changed the
codeofvalue-of/k? Theprocedurevalue-of/kcallsapply-conttail-
recursively, and apply-cont calls apply-procedure/k tail-recursively,
so any result of apply-procedure/k could appear as the result of
value-of/k. And, ofcourse, we havemodified apply-procedure/kto
returnsomethingdifferentthanitdidbefore.
We introduce the set Bounce for the possible results of the value-of/k.
(WecallitBouncebecauseitistheinputtotrampoline.) Whatkindofval-
uescouldappearinthisset? value-of/kcallsitselfandapply-conttail-
recursively, and these are the only tail-recursive calls it makes. So the only
values that could appear as results of value-of/k are those that appear
asresultsofapply-cont. Also, apply-procedure/kcallsvalue-of/k
tail-recursively,sowhateverBounceis,itisthesetofresultsofvalue-of/k,
apply-cont,andapply-procedure/k.

158 5 Continuation-PassingInterpreters
Theproceduresvalue-of/kandapply-contjustcallotherprocedures
tail-recursively. The only procedure that actually puts values in Bounce is
apply-procedure/k. What kind of values are these? Let’s look at the
code.
(define apply-procedure/k
| (lambda (proc1 | val | cont) |     |
| -------------- | --- | ----- | --- |
(lambda ()
| (cases procedure |     | proc1     |     |
| ---------------- | --- | --------- | --- |
| (... (value-of/k |     | ...)))))) |     |
We seethat apply-procedure/kreturnsaprocedureof no arguments,
which when called returns either an ExpValor the result of a call to one of
value-of/k,apply-cont,orapply-procedure/k,thatis,aBounce. So
thepossiblevaluesofapply-procedure/karedescribedbytheset
|     | ExpVal | ∪ (() | → Bounce) |
| --- | ------ | ----- | --------- |
These are the same asthe possible resultsof value-of/k, so we conclude
that
|     |        | =      | ∪ →         |
| --- | ------ | ------ | ----------- |
|     | Bounce | ExpVal | (() Bounce) |
andthatthecontractsshouldbe
→
| value-of-program:  |        | Program       | FinalAnswer            |
| ------------------ | ------ | ------------- | ---------------------- |
| trampoline:        | Bounce | → FinalAnswer |                        |
|                    |        | × ×           | →                      |
| value-of/k:        | Exp    | Env Cont      | Bounce                 |
|                    |        | ×             | →                      |
| apply-cont:        | Cont   | ExpVal        | Bounce                 |
| apply-procedure/k: |        | Proc ×        | ExpVal × Cont → Bounce |
trampoline
The procedure satisfies its contract: it is initially passed a
Bounce. IfitsargumentisanExpVal(andhenceaFinalAnswer),thenitreturns
it. OtherwisetheargumentmustbeaprocedurethatreturnsaBounce. Soit
invokes the procedure on no arguments, and calls itself with the resulting
value, which will always be a Bounce. (We will see in section 7.4 how to
automatereasoninglikethis.)
Eachzero-argumentprocedurereturnedbyapply-procedure/krepre-
sentsasnapshotofthecomputationinprogress. Wecouldchoosetoreturn
suchasnapshotatdifferentplacesinthecomputation; weseeinsection5.5
how this idea can be utilized to simulate atomic actions in multithreaded
programs.

5.2 ATrampolinedInterpreter 159
| Bounce           | = ExpVal         | ∪ (()     | → Bounce)     |     |     |
| ---------------- | ---------------- | --------- | ------------- | --- | --- |
| value-of-program |                  | : Program | → FinalAnswer |     |     |
| (define          | value-of-program |           |               |     |     |
| (lambda          | (pgm)            |           |               |     |     |
| (cases           | program          |           | pgm           |     |     |
|                  | (a-program       | (exp)     |               |     |     |
(trampoline
|     | (value-of/k |     | exp (init-env) |     | (end-cont))))))) |
| --- | ----------- | --- | -------------- | --- | ---------------- |
→
| trampoline | :          | Bounce  | FinalAnswer |     |     |
| ---------- | ---------- | ------- | ----------- | --- | --- |
| (define    | trampoline |         |             |     |     |
| (lambda    | (bounce)   |         |             |     |     |
| (if        | (expval?   | bounce) |             |     |     |
bounce
|                   | (trampoline             | (bounce)))))                           |           |          |          |
| ----------------- | ----------------------- | -------------------------------------- | --------- | -------- | -------- |
|                   |                         | ×                                      | ×         | →        |          |
| value-of/k        | : Exp                   | Env                                    | Cont      | Bounce   |          |
| (define           | value-of/k              |                                        |           |          |          |
| (lambda           | (exp                    | env                                    | cont)     |          |          |
| (cases            | expression              |                                        | exp       |          |          |
|                   | (... (value-of/k        |                                        | ...))     |          |          |
|                   | (... (apply-cont        |                                        | ...)))))  |          |          |
|                   |                         | ×                                      | →         |          |          |
| apply-cont        | : Cont                  | ExpVal                                 |           | Bounce   |          |
| (define           | apply-cont              |                                        |           |          |          |
| (lambda           | (cont                   | val)                                   |           |          |          |
| (cases            | continuation            |                                        | cont      |          |          |
|                   | (... val)               |                                        |           |          |          |
|                   | (... (value-of/k        |                                        | ...))     |          |          |
|                   | (... (apply-cont        |                                        | ...))     |          |          |
|                   | (... (apply-procedure/k |                                        |           | ...))))) |          |
| apply-procedure/k |                         | : Proc                                 | × ExpVal  | × Cont   | → Bounce |
| (define           | apply-procedure/k       |                                        |           |          |          |
| (lambda           | (proc1                  | val                                    | cont)     |          |          |
| (lambda           |                         | ()                                     |           |          |          |
|                   | (cases                  | procedure                              | proc1     |          |          |
|                   | (...                    | (value-of/k                            | ...)))))) |          |          |
|                   | Figure5.7               | Proceduralrepresentationoftrampolining |           |          |          |

160 5 Continuation-PassingInterpreters
(cid:3)
Exercise5.17 [ ] Modify the trampolined interpreter to wrap (lambda () ...)
aroundeachcall(there’sonlyone)toapply-procedure/k.Doesthismodification
requirechangingthecontracts?
(cid:3)
Exercise5.18 [ ]Thetrampolinesysteminfigure5.7usesaproceduralrepresentation
ofaBounce.Replacethisbyadatastructurerepresentation.
(cid:3)
Exercise5.19 [ ]Instead of placing the (lambda () ...) around the body of
apply-procedure/k,placeitaroundthebodyofapply-cont. Modifythe con-
tracts to match this change. Does the definition of Bounce need to change? Then
replacetheproceduralrepresentationofBouncewithadata-structurerepresentation,
asinexercise5.18.
(cid:3)
Exercise5.20 [ ] In exercise 5.19, the last bounce before trampoline returns a
FinalAnswerisalwayssomethinglike(apply-cont (end-cont) val),whereval
is some ExpVal. Optimize your representation of bounces in exercise 5.19 to take
advantageofthisfact.
(cid:3)(cid:3)
Exercise5.21 [ ] Implementatrampolininginterpreterinanordinaryprocedural
language.Useadatastructurerepresentationofthesnapshotsasinexercise5.18,and
replacetherecursivecalltotrampolineinitsownbodybyanordinarywhileor
otherloopingconstruct.
(cid:3)(cid:3)(cid:3)
Exercise5.22 [ ] One could also attempt to transcribe the environment-passing
interpretersof chapter 3 in an ordinary procedurallanguage. Such a transcription
wouldfailinallbutthesimplestcases,forthesamereasonsassuggestedabove.Can
thetechniqueoftrampoliningbeusedinthissituationaswell?
5.3 AnImperativeInterpreter
In chapter 4, we saw how assignment to shared variablescould sometimes
be used in place of binding. Consider the familiar example of even and
odd at the top of figure 5.8. It could be replaced by the program below it
in figure 5.8. There the shared variable x allows communication between
the two procedures. In the top example, the procedure bodies look for the
relevant data in the environment; in the other program, they look for the
relevantdatainthestore.
Consideratraceofthecomputationatthebottomoffigure5.8. Thiscould
beatraceofeithercomputation. Itcouldbeatraceofthefirstcomputation,
inwhichwekeeptrackoftheprocedurebeingcalledanditsargument,orit
couldbeatraceofthesecond,inwhichwekeeptrackoftheprocedurebeing
calledandthecontentsoftheregisterx.
Yetathirdinterpretationofthistracewouldbeasthetraceofgotos(called
aflowchartprogram),inwhichwekeeptrackofthelocationoftheprogram
counterandthecontentsoftheregisterx.

5.3 AnImperativeInterpreter 161
letrec
| even(x) | = if zero?(x) |     |     |
| ------- | ------------- | --- | --- |
then 1
|        | else (odd     | sub1(x)) |     |
| ------ | ------------- | -------- | --- |
| odd(x) | = if zero?(x) |          |     |
then 0
|         | else (even | sub1(x)) |     |
| ------- | ---------- | -------- | --- |
| in (odd | 13)        |          |     |
| let x = | 0          |          |     |
in letrec
| even() | = if zero?(x) |             |             |
| ------ | ------------- | ----------- | ----------- |
|        | then          | 1           |             |
|        | else          | let d = set | x = sub1(x) |
in (odd)
| odd() | = if zero?(x) |     |     |
| ----- | ------------- | --- | --- |
then 0
|        | else let      | d = set x | = sub1(x) |
| ------ | ------------- | --------- | --------- |
|        | in            | (even)    |           |
| in let | d = set       | x = 13    |           |
| in     | (odd)         |           |           |
|        | x = 13;       |           |           |
|        | goto odd;     |           |           |
| even:  | if (x=0) then | return(1) |           |
|        | else          | {x = x-1; |           |
goto odd;}
| odd: | if (x=0) then | return(0) |     |
| ---- | ------------- | --------- | --- |
|      | else          | {x = x-1; |     |
goto even;}
| (odd    | 13) |     |     |
| ------- | --- | --- | --- |
| = (even | 12) |     |     |
| = (odd  | 11) |     |     |
...
| = (odd  | 1)  |     |     |
| ------- | --- | --- | --- |
| = (even | 0)  |     |     |
= 1
|     | Figure5.8 Threeprogramswithacommontrace |     |     |
| --- | --------------------------------------- | --- | --- |

162 5 Continuation-PassingInterpreters
Butthisworksonlybecauseintheoriginalcodethecallstoevenandodd
donot growanycontrolcontext: theyaretailcalls. We could not carryout
thistransformationforfact,becausethetraceoffactgrowsunboundedly:
the“programcounter”appearsnotattheoutsideofthetrace,asitdoeshere,
butinsideacontrolcontext.
We can carry out this transformation for any procedure that does not
| requirecontrolcontext. | Thisleadsustoanimportantprinciple: |     |     |
| ---------------------- | ---------------------------------- | --- | --- |
A0-argumenttailcallisthesameasajump.
If a group of procedures call each other only by tail calls, then we can
translate the calls to use assignment instead of binding, and then we can
translatesuch an assignment programinto a flowchart program, aswe did
infigure5.8.
In this section, we will use this principle to translate the continuation-
passing interpreter into a form suitable for transcription into a language
withouthigher-orderprocedures.
Webeginwiththeinterpreteroffigures5.4and5.5,usingadatastructure
representationofcontinuations. Thedatastructurerepresentationisshown
infigures5.9and5.10.
Our first task is to list the procedures that will communicate via shared
registers.Theseprocedures,withtheirformalparameters,are:
| (value-of/k exp    | env cont) |       |     |
| ------------------ | --------- | ----- | --- |
| (apply-cont cont   | val)      |       |     |
| (apply-procedure/k | proc1 val | cont) |     |
So we will need five global registers: exp, env, cont, val, and proc1.
Eachofthethreeproceduresabovewillbereplacedbyazero-argumentpro-
cedure,andeachcalltooneoftheseprocedureswillbereplacedbycodethat
storesthe valueof eachactualparameterin the corresponding registerand
theninvokesthenewzero-argumentprocedure.Sothefragment
(define value-of/k
| (lambda (exp      | env cont)         |               |        |
| ----------------- | ----------------- | ------------- | ------ |
| (cases expression | exp               |               |        |
| (const-exp        | (num) (apply-cont | cont (num-val | num))) |
...)))
canbereplacedby

5.3 AnImperativeInterpreter 163
| (define-datatype | continuation | continuation? |
| ---------------- | ------------ | ------------- |
(end-cont)
(zero1-cont
| (saved-cont | continuation?)) |     |
| ----------- | --------------- | --- |
(let-exp-cont
(var identifier?)
| (body expression?) |                 |     |
| ------------------ | --------------- | --- |
| (saved-env         | environment?)   |     |
| (saved-cont        | continuation?)) |     |
(if-test-cont
| (exp2 expression?) |                 |     |
| ------------------ | --------------- | --- |
| (exp3 expression?) |                 |     |
| (saved-env         | environment?)   |     |
| (saved-cont        | continuation?)) |     |
(diff1-cont
| (exp2 expression?) |                 |     |
| ------------------ | --------------- | --- |
| (saved-env         | environment?)   |     |
| (saved-cont        | continuation?)) |     |
(diff2-cont
| (val1 expval?) |                 |     |
| -------------- | --------------- | --- |
| (saved-cont    | continuation?)) |     |
(rator-cont
| (rand expression?) |                 |     |
| ------------------ | --------------- | --- |
| (saved-env         | environment?)   |     |
| (saved-cont        | continuation?)) |     |
(rand-cont
| (val1 expval?)                                              |                  |     |
| ----------------------------------------------------------- | ---------------- | --- |
| (saved-cont                                                 | continuation?))) |     |
| Figure5.9 Datastructureimplementationofcontinuations(part1) |                  |     |
(define value-of/k
| (lambda ()        |       |     |
| ----------------- | ----- | --- |
| (cases expression | exp   |     |
| (const-exp        | (num) |     |
(set! cont cont)
| (set! | val (num-val | num)) |
| ----- | ------------ | ----- |
(apply-cont))
...)))
We can now systematically go through each of our four procedures and
perform this transformation. We will also have to transform the body of

| 164        |           |              |        |             | 5   | Continuation-PassingInterpreters |
| ---------- | --------- | ------------ | ------ | ----------- | --- | -------------------------------- |
|            |           | :            | ×      | →           |     |                                  |
| apply-cont |           | Cont         | ExpVal | FinalAnswer |     |                                  |
| (define    |           | apply-cont   |        |             |     |                                  |
| (lambda    |           | (cont        | val)   |             |     |                                  |
|            | (cases    | continuation |        | cont        |     |                                  |
|            | (end-cont |              | ()     |             |     |                                  |
(begin
(eopl:printf
|     |     | "End | of computation.~%") |     |     |     |
| --- | --- | ---- | ------------------- | --- | --- | --- |
val))
|     | (zero1-cont |             | (saved-cont) |     |     |     |
| --- | ----------- | ----------- | ------------ | --- | --- | --- |
|     |             | (apply-cont | saved-cont   |     |     |     |
(bool-val
|     |               | (zero?                                            | (expval->num      |                | val)))))          |                  |
| --- | ------------- | ------------------------------------------------- | ----------------- | -------------- | ----------------- | ---------------- |
|     | (let-exp-cont |                                                   | (var              | body           | saved-env         | saved-cont)      |
|     |               | (value-of/k                                       | body              |                |                   |                  |
|     |               | (extend-env                                       | var               | val            | saved-env)        | saved-cont))     |
|     | (if-test-cont |                                                   | (exp2             | exp3           | saved-env         | saved-cont)      |
|     |               | (if (expval->bool                                 |                   | val)           |                   |                  |
|     |               | (value-of/k                                       | exp2              | saved-env      |                   | saved-cont)      |
|     |               | (value-of/k                                       | exp3              | saved-env      |                   | saved-cont)))    |
|     | (diff1-cont   |                                                   | (exp2             | saved-env      | saved-cont)       |                  |
|     |               | (value-of/k                                       | exp2              |                |                   |                  |
|     |               | saved-env                                         | (diff2-cont       |                | val saved-cont))) |                  |
|     | (diff2-cont   |                                                   | (val1             | saved-cont)    |                   |                  |
|     |               | (let ((num1                                       | (expval->num      |                | val1))            |                  |
|     |               | (num2                                             | (expval->num      |                | val)))            |                  |
|     |               | (apply-cont                                       | saved-cont        |                |                   |                  |
|     |               | (num-val                                          | (-                | num1 num2))))) |                   |                  |
|     | (rator-cont   |                                                   | (rand             | saved-env      | saved-cont)       |                  |
|     |               | (value-of/k                                       | rand              | saved-env      |                   |                  |
|     |               | (rand-cont                                        | val               | saved-cont)))  |                   |                  |
|     | (rand-cont    |                                                   | (val1 saved-cont) |                |                   |                  |
|     |               | (let ((proc                                       | (expval->proc     |                | val1)))           |                  |
|     |               | (apply-procedure/k                                |                   | proc           | val               | saved-cont)))))) |
|     | Figure5.10    | Datastructureimplementationofcontinuations(part2) |                   |                |                   |                  |

5.3 AnImperativeInterpreter 165
value-of-program, since that is where value-of/k is initially called.
Therearejustthreeeasy-to-resolvecomplications:
1. Often a register is unchanged from one procedure call to another. This
| yieldsanassignmentlike(set! | cont cont)intheexampleabove.We |     |
| --------------------------- | ------------------------------ | --- |
cansafelyomitsuchassignments.
2. We must make sure that no field name in a cases expression happens
to be the same as a register name. In this situation, the field shad-
ows the register, so the register becomes inaccessible. For example, if in
value-of-programwehadwritten
| (cases program | pgm            |               |
| -------------- | -------------- | ------------- |
| (a-program     | (exp)          |               |
| (value-of/k    | exp (init-env) | (end-cont)))) |
exp
then would be locally bound, so we could not assign to the global
register exp. The solution is to rename the local variable to avoid the
conflict:
| (cases program | pgm             |               |
| -------------- | --------------- | ------------- |
| (a-program     | (exp1)          |               |
| (value-of/k    | exp1 (init-env) | (end-cont)))) |
Thenwecanwrite
| (cases program | pgm         |     |
| -------------- | ----------- | --- |
| (a-program     | (exp1)      |     |
| (set! cont     | (end-cont)) |     |
| (set! exp      | exp1)       |     |
| (set! env      | (init-env)) |     |
(value-of/k)))
We have already carefully chosen the field names in our data types to
avoidsuchconflicts.

166 5 Continuation-PassingInterpreters
3. An additionalcomplication mayarise if a registeris used twice in a sin-
gle call. Consider transforming a first call in (cons (f (car x)) (f
(cdr x))), where x is the formalparameterof f. A naive transforma-
tionofthiscallwouldbe:
(begin
(set! x (car x))
(set! cont (arg1-cont x cont))
(f))
Butthisisincorrect,becauseitloadstheregisterxwiththenewvalueof
x,whentheoldvalueofxwasintended. Thesolutioniseithertoreorder
the assignments so the right values are loaded into the registers, or to
usetemporaryvariables. Mostoccurrencesofthisbugcanbeavoidedby
assigningtothecontinuationvariablefirst:
(begin
(set! cont (arg1-cont x cont))
(set! x (car x))
(f))
Occasionally, temporary variables are unavoidable; consider (f y x)
where x and y are the formalparametersof f. Again, this complication
doesnotariseinourexample.
The result of performing this translation on our interpreter is shown in
figures5.11–5.14. Thisprocessiscalledregisterization. Itisaneasyprocessto
translatethisintoanimperativelanguagethatsupportsgotos.
(cid:3)
Exercise5.23 [ ]Whathappensifyouremovethe“goto”lineinoneofthebranches
ofthisinterpreter?Exactlyhowdoestheinterpreterfail?
(cid:3)
Exercise5.24 [ ]Deviseexamplestoillustrateeachofthecomplicationsmentioned
above.
(cid:3)(cid:3)
Exercise5.25 [ ] Registerize the interpreter for multiargument procedures (exer-
cise3.21).
(cid:3)
Exercise5.26 [ ] Convert this interpreter to a trampoline by replacing each call to
apply-procedure/kwith(set! pc apply-procedure/k)andusingadriver
thatlookslike
(define trampoline
(lambda (pc)
(if pc (trampoline (pc)) val)))

5.3 AnImperativeInterpreter 167
| (define          | exp              | ’uninitialized)  |               |     |     |
| ---------------- | ---------------- | ---------------- | ------------- | --- | --- |
| (define          | env              | ’uninitialized)  |               |     |     |
| (define          | cont             | ’uninitialized)  |               |     |     |
| (define          | val              | ’uninitialized)  |               |     |     |
| (define          | proc1            | ’uninitialized)  |               |     |     |
| value-of-program |                  | : Program        | → FinalAnswer |     |     |
| (define          | value-of-program |                  |               |     |     |
| (lambda          | (pgm)            |                  |               |     |     |
| (cases           |                  | program pgm      |               |     |     |
|                  | (a-program       | (exp1)           |               |     |     |
|                  | (set!            | cont (end-cont)) |               |     |     |
|                  | (set!            | exp exp1)        |               |     |     |
|                  | (set!            | env (init-env))  |               |     |     |
(value-of/k)))))
→
| value-of/k | :          | () FinalAnswer |       |     |     |
| ---------- | ---------- | -------------- | ----- | --- | --- |
| usage:     | : relies   | on registers   |       |     |     |
|            | exp        | : Exp          |       |     |     |
|            | env        | : Env          |       |     |     |
|            | cont       | : Cont         |       |     |     |
| (define    | value-of/k |                |       |     |     |
| (lambda    | ()         |                |       |     |     |
| (cases     |            | expression exp |       |     |     |
|            | (const-exp | (num)          |       |     |     |
|            | (set!      | val (num-val   | num)) |     |     |
(apply-cont))
|     | (var-exp | (var)          |     |       |     |
| --- | -------- | -------------- | --- | ----- | --- |
|     | (set!    | val (apply-env | env | var)) |     |
(apply-cont))
|     | (proc-exp | (var body)    |            |          |        |
| --- | --------- | ------------- | ---------- | -------- | ------ |
|     | (set!     | val (proc-val | (procedure | var body | env))) |
(apply-cont))
|     | (letrec-exp | (p-name             | b-var | p-body letrec-body) |              |
| --- | ----------- | ------------------- | ----- | ------------------- | ------------ |
|     | (set!       | exp letrec-body)    |       |                     |              |
|     | (set!       | env (extend-env-rec |       | p-name b-var        | p-body env)) |
(value-of/k))
Figure5.11
Imperativeinterpreter(part1)

168 5 Continuation-PassingInterpreters
| (zero?-exp | (exp1)      |        |     |
| ---------- | ----------- | ------ | --- |
| (set! cont | (zero1-cont | cont)) |     |
| (set! exp  | exp1)       |        |     |
(value-of/k))
| (let-exp (var | exp1 body)    |          |            |
| ------------- | ------------- | -------- | ---------- |
| (set! cont    | (let-exp-cont | var body | env cont)) |
| (set! exp     | exp1)         |          |            |
(value-of/k))
| (if-exp (exp1 | exp2 exp3)    |           |            |
| ------------- | ------------- | --------- | ---------- |
| (set! cont    | (if-test-cont | exp2 exp3 | env cont)) |
| (set! exp     | exp1)         |           |            |
(value-of/k))
| (diff-exp  | (exp1 exp2) |          |        |
| ---------- | ----------- | -------- | ------ |
| (set! cont | (diff1-cont | exp2 env | cont)) |
| (set! exp  | exp1)       |          |        |
(value-of/k))
| (call-exp  | (rator rand) |          |        |
| ---------- | ------------ | -------- | ------ |
| (set! cont | (rator-cont  | rand env | cont)) |
| (set! exp  | rator)       |          |        |
(value-of/k)))))
| Figure5.12 | Imperativeinterpreter(part2) |     |     |
| ---------- | ---------------------------- | --- | --- |
(cid:3)
Exercise5.27 [ ] Inventalanguagefeatureforwhichsettingthecontvariablelast
requiresatemporaryvariable.
(cid:3)
Exercise5.28 [ ] Instrumentthisinterpreterasinexercise5.12. Sincecontinuations
arerepresentedthesameway,reusethatcode. Verifythattheimperativeinterpreter
ofthissectiongeneratesexactlythesametracesastheinterpreterinexercise5.12.
(cid:3)
Exercise5.29 [ ]Applythetransformationofthissectiontofact-iter(page139).
Exercise5.30 [ (cid:3)(cid:3) ] Modifythe interpreterof this sectionso that proceduresrelyon
dynamicbinding,asinexercise3.28.Asahint,considertransformingtheinterpreter
of exercise 3.28 as we did in this chapter; it will differ from the interpreter of this
sectiononlyforthoseportionsoftheoriginalinterpreterthataredifferent.Instrument
theinterpreterasinexercise5.28.Observethatjustasthereisonlyonecontinuationin
thestate,thereisonlyoneenvironmentthatispushedandpopped,andfurthermore,
it is pushed and popped in parallel with the continuation. We can conclude that
dynamicbindingshavedynamicextent: thatis,abindingtoaformalparameterlasts
exactlyuntilthatprocedurereturns.Thisisdifferentfromlexicalbindings,whichcan
persistindefinitelyiftheywindupinaclosure.

5.3 AnImperativeInterpreter 169
→
| apply-cont : () | FinalAnswer |     |     |
| --------------- | ----------- | --- | --- |
| usage: : reads  | registers   |     |     |
cont : Cont
val : ExpVal
(define apply-cont
(lambda ()
| (cases continuation | cont |                     |     |
| ------------------- | ---- | ------------------- | --- |
| (end-cont           | ()   |                     |     |
| (eopl:printf        | "End | of computation.~%") |     |
val)
| (zero1-cont | (saved-cont) |                     |         |
| ----------- | ------------ | ------------------- | ------- |
| (set! cont  | saved-cont)  |                     |         |
| (set! val   | (bool-val    | (zero? (expval->num | val)))) |
(apply-cont))
| (let-exp-cont | (var        | body saved-env | saved-cont) |
| ------------- | ----------- | -------------- | ----------- |
| (set! cont    | saved-cont) |                |             |
| (set! exp     | body)       |                |             |
| (set! env     | (extend-env | var val        | saved-env)) |
(value-of/k))
| (if-test-cont     | (exp2       | exp3 saved-env | saved-cont) |
| ----------------- | ----------- | -------------- | ----------- |
| (set! cont        | saved-cont) |                |             |
| (if (expval->bool |             | val)           |             |
| (set!             | exp exp2)   |                |             |
| (set!             | exp exp3))  |                |             |
| (set! env         | saved-env)  |                |             |
(value-of/k))
| Figure5.13 | Imperativeinterpreter(part3) |     |     |
| ---------- | ---------------------------- | --- | --- |
Exercise5.31 [ (cid:3) ] Eliminatetheremainingletexpressionsinthiscodebyusingaddi-
tionalglobalregisters.
(cid:3)(cid:3)
Exercise5.32 [ ] Improve your solution to the precedingexerciseby minimizing
thenumberofglobalregistersused. Youcangetawaywithfewerthan5. Youmay
usenodatastructuresotherthanthosealreadyusedbytheinterpreter.
(cid:3)(cid:3)
Exercise5.33 [ ] Translate the interpreter of this section into an imperative lan-
guage.Dothistwice:onceusingzero-argumentprocedurecallsinthehostlanguage,
and once replacing each zero-argument procedure call by a goto. How do these
alternativesperformasthecomputationgetslonger?

170 5 Continuation-PassingInterpreters
| (diff1-cont | (exp2            | saved-env | saved-cont)      |     |
| ----------- | ---------------- | --------- | ---------------- | --- |
| (set!       | cont (diff2-cont |           | val saved-cont)) |     |
| (set!       | exp exp2)        |           |                  |     |
| (set!       | env saved-env)   |           |                  |     |
(value-of/k))
| (diff2-cont | (val1              | saved-cont) |         |         |
| ----------- | ------------------ | ----------- | ------- | ------- |
| (let ((num1 | (expval->num       |             | val1))  |         |
|             | (num2 (expval->num |             | val)))  |         |
| (set!       | cont saved-cont)   |             |         |         |
| (set!       | val (num-val       |             | (- num1 | num2))) |
(apply-cont)))
| (rator-cont | (rand           | saved-env | saved-cont)      |     |
| ----------- | --------------- | --------- | ---------------- | --- |
| (set!       | cont (rand-cont |           | val saved-cont)) |     |
| (set!       | exp rand)       |           |                  |     |
| (set!       | env saved-env)  |           |                  |     |
(value-of/k))
| (rand-cont        | (rator-val        |               | saved-cont) |              |
| ----------------- | ----------------- | ------------- | ----------- | ------------ |
| (let ((rator-proc |                   | (expval->proc |             | rator-val))) |
| (set!             | cont saved-cont)  |               |             |              |
| (set!             | proc1 rator-proc) |               |             |              |
| (set!             | val val)          |               |             |              |
(apply-procedure/k))))))
→
| apply-procedure/k | : ()         | FinalAnswer |     |     |
| ----------------- | ------------ | ----------- | --- | --- |
| usage: : relies   | on registers |             |     |     |
| proc1 :           | Proc         |             |     |     |
val :
ExpVal
| cont : | Cont |     |     |     |
| ------ | ---- | --- | --- | --- |
(define apply-procedure/k
(lambda ()
| (cases proc | proc1           |            |         |             |
| ----------- | --------------- | ---------- | ------- | ----------- |
| (procedure  | (var body       | saved-env) |         |             |
| (set!       | exp body)       |            |         |             |
| (set!       | env (extend-env |            | var val | saved-env)) |
(value-of/k)))))
| Figure5.14 |     | Imperativeinterpreter(part4) |     |     |
| ---------- | --- | ---------------------------- | --- | --- |

5.4 Exceptions 171
(cid:3)(cid:3)
Exercise5.34 [ ] Asnotedonpage157,mostimperativelanguagesmakeitdifficult
to do this translation, because they use the stack for all procedure calls, even tail
calls. Furthermore, for largeinterpreters,the pieces of code linkedby goto’smay
be too large for some compilersto handle. Translate the interpreter of this section
intoanimperativelanguage,circumventingthisdifficultybyusingthetechniqueof
trampolining,asinexercise5.26.
5.4 Exceptions
Sofarwehaveusedcontinuationsonlytomanagetheordinaryflowofcon-
trolinourlanguages. Butcontinuationsallowustoalterthecontrolcontext
as well. Letus consider adding exceptionhandling to our defined language.
Weaddtothelanguagetwonewproductions:
| Expression::=try   | Expression catch  | (Identifier) Expression |     |
| ------------------ | ----------------- | ----------------------- | --- |
|                    | try-exp (exp1 var | handler-exp)            |     |
| Expression::=raise | Expression        |                         |     |
raise-exp (exp)
Atryexpressionevaluatesitsfirstargumentinthecontextoftheexcep-
tion handler described bythe catch clause. If this expressionreturns nor-
mally, its value becomes the value of the entire try expression, and the
exceptionhandlerisremoved.
Araiseexpressionevaluatesitssingleexpressionandraisesanexception
with that value. The value is sent to the most recently installed exception
handler and is bound to the variable of the handler. The body of the han-
dler is then evaluated. The handler body can either return a value, which
becomes the value of the associated try expression, or it can propagatethe
exceptionbyraisinganother exception; inthis case the exceptionwould be
senttothenextmostrecentlyinstalledexceptionhandler.
Here’sanexample,whereweassumeforthemomentthatwehaveadded
stringstothelanguage.
| let list-index | =   |     |     |
| -------------- | --- | --- | --- |
proc (str)
|     | letrec inner (lst) |     |     |
| --- | ------------------ | --- | --- |
= if null?(lst)
then raise("ListIndexFailed")
|     | else if string-equal?(car(lst), |            | str) |
| --- | ------------------------------- | ---------- | ---- |
|     | then 0                          |            |      |
|     | else -((inner                   | cdr(lst)), | -1)  |

172 5 Continuation-PassingInterpreters
The procedurelist-indexis a Curried procedurethattakes a string and
list of strings, and returns the position of the string in the list. If the
desired list element is not found, inner raises an exception and passes
"ListIndexFailed"tothemostrecentlyinstalledhandler,skippingover
allthependingsubtractions.
Thehandlercantakeadvantageofknowledgeatthecallsitetohandlethe
exceptionappropriately.
let find-member-number =
proc (member-name)
... try ((list-index member-name) member-list)
catch (exn)
raise("CantFindMemberNumber")
Theprocedurefind-member-numbertakesastringanduseslist-index
to find the position of the string in the list member-list. The caller
of find-member-number has no reason to know about list-index, so
find-member-numbertranslatesthe errormessage intoanexceptionthat
itscallercanunderstand.
Yetanother possibility, dependingonthe purpose ofthe program, isthat
find-member-numbermightreturnsomedefaultnumberifthemember’s
nameisnotinthelist.
let find-member-number =
proc (member-name)
... try ((list-index member-name) member-list)
catch (exn)
the-default-member-number
Inboththeseprograms,wehaveignoredthevalueoftheexception. Inother
situations, the valuepassedbyraisemight include some partialinforma-
tionthatthecallercouldutilize.
Implementingthisexception-handlingmechanismusingthecontinuation-
passinginterpreterisstraightforward.Webeginwiththetryexpression. In
thedata-structurerepresentation,weaddtwocontinuation-builders:
(try-cont
(var identifier?)
(handler-exp expression?)
(env environment?)
(cont continuation?))
(raise1-cont
(saved-cont continuation?))

5.4 Exceptions 173
andweaddtovalue-of/kthefollowingclausefortry:
| (try-exp | (exp1       | var  | handler-exp) |     |         |
| -------- | ----------- | ---- | ------------ | --- | ------- |
|          | (value-of/k | exp1 | env          |     |         |
|          | (try-cont   | var  | handler-exp  | env | cont))) |
What happenswhen the body of the try expressionis evaluated? If the
bodyreturnsnormally,thenthatvalueshouldbesenttothecontinuationof
thetryexpression,inthiscasecont:
| (apply-cont   | (try-cont |      |                 |     | cont) val) |
| ------------- | --------- | ---- | --------------- | --- | ---------- |
|               |           |      | var handler-exp | env |            |
| = (apply-cont | cont      | val) |                 |     |            |
Whathappensifanexceptionisraised?First,ofcourse,weneedtoevalu-
atetheargumenttoraise.
| (raise-exp |              | (exp1) |         |     |     |
| ---------- | ------------ | ------ | ------- | --- | --- |
|            | (value-of/k  | exp1   | env     |     |     |
|            | (raise1-cont |        | cont))) |     |     |
Whenthevalueofexp1isreturnedtoraise1-cont,weneedtosearch
throughthecontinuationforthenearesthandler,whichmaybefoundinthe
topmosttry-contcontinuation.
Sointhespecificationofcontinuationswe
write
| (apply-cont      | (raise1-cont |     | cont) | val) |     |
| ---------------- | ------------ | --- | ----- | ---- | --- |
| = (apply-handler |              | val | cont) |      |     |
whereapply-handlerisaprocedurethatfindstheclosestexceptionhan-
dlerandappliesit(figure5.15).
To show how all this fits together, let us consider a calculation using a
definedlanguageimplementationofindex.
|     |       |     |     | Letexp | 0 denotetheexpression |
| --- | ----- | --- | --- | ------ | --------------------- |
| let | index |     |     |        |                       |
= proc (n)
|     | letrec | inner      | (lst)                |     |     |
| --- | ------ | ---------- | -------------------- | --- | --- |
|     | = if   | null?(lst) |                      |     |     |
|     | then   | raise      | 99                   |     |     |
|     | else   | if         | zero?(-(car(lst),n)) |     |     |
then 0
|     |            | else    | -((inner | cdr(lst)), | -1) |
| --- | ---------- | ------- | -------- | ---------- | --- |
|     | in proc    | (lst)   |          |            |     |
|     | try        | (inner  | lst)     |            |     |
|     |            | catch   | (x) -1   |            |     |
| in  | ((index 5) | list(2, | 3))      |            |     |

174 5 Continuation-PassingInterpreters
|               |               |              | ×                | →       |             |             |
| ------------- | ------------- | ------------ | ---------------- | ------- | ----------- | ----------- |
| apply-handler |               | : ExpVal     |                  | Cont    | FinalAnswer |             |
| (define       | apply-handler |              |                  |         |             |             |
| (lambda       |               | (val         | cont)            |         |             |             |
|               | (cases        | continuation |                  | cont    |             |             |
|               | (try-cont     |              | (var handler-exp |         | saved-env   | saved-cont) |
|               | (value-of/k   |              | handler-exp      |         |             |             |
|               |               | (extend-env  |                  | var val | saved-env)  |             |
saved-cont))
|     | (end-cont |     | ()  |     |     |     |
| --- | --------- | --- | --- | --- | --- | --- |
(report-uncaught-exception))
|     | (diff1-cont    |     | (exp2 | saved-env        | saved-cont) |     |
| --- | -------------- | --- | ----- | ---------------- | ----------- | --- |
|     | (apply-handler |     |       | val saved-cont)) |             |     |
|     | (diff2-cont    |     | (val1 | saved-cont)      |             |     |
|     | (apply-handler |     |       | val saved-cont)) |             |     |
...)))
Theprocedureapply-handler
Figure5.15
Westartexp inanarbitraryenvironment ρ andanarbitrarycontinuation
|     |     | 0   |     |     | 0   |     |
| --- | --- | --- | --- | --- | --- | --- |
cont . We will show only the highlights of the calculation, with comments
0
interspersed.
(value-of/k
| <<let | index | = ... | in  | ((index | 5) list(2, | 3))>> |
| ----- | ----- | ----- | --- | ------- | ---------- | ----- |
ρ
0
cont )
0
= executethebodyofthelet
(value-of/k
| <<((index |     | 5) list(2, | 3))>> |     |     |     |
| --------- | --- | ---------- | ----- | --- | --- | --- |
callthisρ
((index
1
#(struct:proc-val
|     | #(struct:procedure |     |     | n <<letrec | ...>> | ρ ))) |
| --- | ------------------ | --- | --- | ---------- | ----- | ----- |
0
| (i  | #(struct:num-val |     | 1))   |     |     |     |
| --- | ---------------- | --- | ----- | --- | --- | --- |
| (v  | #(struct:num-val |     | 5))   |     |     |     |
| (x  | #(struct:num-val |     | 10))) |     |     |     |
#(struct:end-cont))

| 5.4 Exceptions |     |     |     |     |     |     | 175 |
| -------------- | --- | --- | --- | --- | --- | --- | --- |
= eventuallyweevaluatethetry
(value-of/k
| <<try | (inner2 | lst) | catch |     | (x) -1>> |           |     |
| ----- | ------- | ---- | ----- | --- | -------- | --------- | --- |
| ((lst |         |      |       |     |          | callthisρ |     |
lst=(23)
#(struct:list-val
|         | (#(struct:num-val |      |     | 2)  | #(struct:num-val | 3)))) |     |
| ------- | ----------------- | ---- | --- | --- | ---------------- | ----- | --- |
| (inner2 |                   | ...) |     |     |                  |       |     |
| (n      | #(struct:num-val  |      |     | 5)) |                  |       |     |
ρ )
0
#(struct:end-cont))
= evaluatethebodyofthetryinatry-contcontinuation
(value-of/k
| <<(inner2 |     | lst)>> |     |     |     |     |     |
| --------- | --- | ------ | --- | --- | --- | --- | --- |
ρ
lst=(23)
ρ
| #(struct:try-cont |     |     | x   | <<-1>> | lst=(23) |     |     |
| ----------------- | --- | --- | --- | ------ | -------- | --- | --- |
#(struct:end-cont)))
= evaluatethebodyofinner2withlstboundto(23)
(value-of/k
| <<if | null?(lst) |     | ... | >>  |     |     |     |
| ---- | ---------- | --- | --- | --- | --- | --- | --- |
ρ
lst=(23)
ρ
| #(struct:try-cont |     |     | x   | <<-1>> | lst=(23) |     |     |
| ----------------- | --- | --- | --- | ------ | -------- | --- | --- |
#(struct:end-cont)))
= evaluatetheconditional,gettingtotherecursionline
(value-of/k
| <<-((inner2 |     | cdr(lst)), |     | -1)>> |     |     |     |
| ----------- | --- | ---------- | --- | ----- | --- | --- | --- |
ρ
lst=(23)
ρ
| #(struct:try-cont |     |     | x   | <<-1>> | lst=(23) |     |     |
| ----------------- | --- | --- | --- | ------ | -------- | --- | --- |
#(struct:end-cont)))
= evaluatethefirstargumentofthediff-exp
(value-of/k
| <<(inner2 |     | cdr(lst))>> |     |     |     |     |     |
| --------- | --- | ----------- | --- | --- | --- | --- | --- |
ρ
lst=(23)
ρ
| #(struct:diff1-cont |                   |     |     | <<-1>>   | lst=(23)   |     |     |
| ------------------- | ----------------- | --- | --- | -------- | ---------- | --- | --- |
|                     | #(struct:try-cont |     |     | x <<-1>> | ρ lst=(23) |     |     |
#(struct:end-cont))))
= evaluatethebodyofinner2withlstboundto(3)
(value-of/k
| <<if | null?(lst) |     | ...>> |     |     |     |     |
| ---- | ---------- | --- | ----- | --- | --- | --- | --- |
callthisρ
| ((lst   | #(struct:list-val |      |     | (#(struct:num-val |     | 3)))) | lst=(3) |
| ------- | ----------------- | ---- | --- | ----------------- | --- | ----- | ------- |
| (inner2 |                   | ...) |     |                   |     |       |         |
ρ )
0
| #(struct:diff1-cont |     |     |     | <<-1>> | ρ   |     |     |
| ------------------- | --- | --- | --- | ------ | --- | --- | --- |
lst=(23)
|     | #(struct:try-cont |     |     | x <<-1>> | ρ   |     |     |
| --- | ----------------- | --- | --- | -------- | --- | --- | --- |
lst=(23)
#(struct:end-cont))))

176 5 Continuation-PassingInterpreters
= evaluatetheconditional,gettingtotherecursionlineagain
(value-of/k
| <<-((inner2 | cdr(lst)), | -1)>> |     |     |
| ----------- | ---------- | ----- | --- | --- |
ρ
lst=(3)
| #(struct:diff1-cont |     | <<-1>> ρ |     |     |
| ------------------- | --- | -------- | --- | --- |
lst=(23)
| #(struct:try-cont |     | x <<-1>> ρ |     |     |
| ----------------- | --- | ---------- | --- | --- |
lst=(23)
#(struct:end-cont))))
= evaluatethefirstargumentofthediff-exp
(value-of/k
| <<(inner2 | cdr(lst))>> |     |     |     |
| --------- | ----------- | --- | --- | --- |
ρ
lst=(3)
| #(struct:diff1-cont |     | <<-1>> ρ |     |     |
| ------------------- | --- | -------- | --- | --- |
lst=(3)
ρ
| #(struct:diff1-cont |     | <<-1>> | lst=(23) |     |
| ------------------- | --- | ------ | -------- | --- |
ρ
| #(struct:try-cont |     | x <<-1>> | lst=(23) |     |
| ----------------- | --- | -------- | -------- | --- |
#(struct:end-cont)))))
= evaluatethebodyofinner2withlstboundto()
(value-of/k
| <<if null?(lst) | ... | >>  |     |     |
| --------------- | --- | --- | --- | --- |
callthisρ
| ((lst #(struct:list-val |      | ())) |     | lst=() |
| ----------------------- | ---- | ---- | --- | ------ |
| (inner2                 | ...) |      |     |        |
| (n #(struct:num-val     |      | 5))  |     |        |
...)
| #(struct:diff1-cont |     | <<-1>> ρ |     |     |
| ------------------- | --- | -------- | --- | --- |
lst=(3)
| #(struct:diff1-cont |     | <<-1>> ρ |     |     |
| ------------------- | --- | -------- | --- | --- |
lst=(23)
| #(struct:try-cont |     | x <<-1>> | ρ   |     |
| ----------------- | --- | -------- | --- | --- |
lst=(23)
#(struct:end-cont)))))
= evaluatetheraiseexpression
(value-of/k
| <<raise 99>> |     |     |     |     |
| ------------ | --- | --- | --- | --- |
ρ
lst=(())
| #(struct:diff1-cont |     | <<-1>> ρ |     |     |
| ------------------- | --- | -------- | --- | --- |
lst=(3)
ρ
| #(struct:diff1-cont |     | <<-1>> | lst=(23) |     |
| ------------------- | --- | ------ | -------- | --- |
ρ
| #(struct:try-cont |     | x <<-1>> | lst=(23) |     |
| ----------------- | --- | -------- | -------- | --- |
#(struct:end-cont)))))
= evaluatetheargumentoftheraiseexpression
(value-of/k
<<99>>
ρ
lst=(())
#(struct:raise1-cont
ρ
| #(struct:diff1-cont |     | <<-1>> | lst=(3) |     |
| ------------------- | --- | ------ | ------- | --- |
| #(struct:diff1-cont |     | <<-1>> | ρ       |     |
lst=(23)
|     | #(struct:try-cont | x <<-1>> | ρ   |     |
| --- | ----------------- | -------- | --- | --- |
lst=(23)
#(struct:end-cont))))))

5.4 Exceptions 177
= useapply-handlertounwindthecontinuationuntilwefindahandler
(apply-handler
| #(struct:num-val    | 99)    |                  |     |
| ------------------- | ------ | ---------------- | --- |
| #(struct:diff1-cont | <<-1>> | ρlst=(3)         |     |
| #(struct:diff1-cont | <<-1>> | ρlst=(23)        |     |
| #(struct:try-cont   | x      | <<-1>> ρlst=(23) |     |
#(struct:end-cont)))))
=
(apply-handler
| #(struct:num-val    | 99)      |           |     |
| ------------------- | -------- | --------- | --- |
| #(struct:diff1-cont | <<-1>>   | ρlst=(23) |     |
| #(struct:try-cont   | x <<-1>> | ρlst=(23) |     |
#(struct:end-cont))))
=
(apply-handler
| #(struct:num-val  | 99)      |           |     |
| ----------------- | -------- | --------- | --- |
| #(struct:try-cont | x <<-1>> | ρlst=(23) |     |
#(struct:end-cont)))
= Handlerfound;bindthevalueoftheexceptiontox
(value-of/k
#(struct:const-exp -1)
((x #(struct:num-val 99))
ρlst=(23)...)
#(struct:end-cont))
=
| (apply-cont | #(struct:end-cont) | #(struct:num-val | -1)) |
| ----------- | ------------------ | ---------------- | ---- |
=
| #(struct:num-val | -1) |     |     |
| ---------------- | --- | --- | --- |
If the list had contained the desired element, then we would have called
apply-continsteadofapply-handler,andwewouldhaveexecutedall
thependingdiffsinthecontinuation.
Exercise5.35 [ (cid:3)(cid:3) ]Thisimplementationisinefficient, becausewhenanexceptionis
raised,apply-handlermustsearchlinearlythroughthecontinuationtofindahan-
Avoidthissearchbymakingthetry-contcontinuation
| dler. |     |     | available directlyin |
| ----- | --- | --- | -------------------- |
eachcontinuation.
(cid:3)
Exercise5.36 [ ]Analternativedesignthatalsoavoidsthelinearsearchinapply-
handleristousetwocontinuations, anormalcontinuation andanexceptioncon-
tinuation. Achievethisgoalbymodifyingtheinterpreterofthissectiontotaketwo
continuationsinsteadofone.
(cid:3)
Exercise5.37 [ ] Modifythe definedlanguagetoraiseanexceptionwhenaproce-
dureiscalledwiththewrongnumberofarguments.
(cid:3)
Exercise5.38 [ ] Modifythedefinedlanguagetoaddadivisionexpression.Raisean
exceptionondivisionbyzero.

178 5 Continuation-PassingInterpreters
(cid:3)(cid:3)
Exercise5.39 [ ] So far, an exceptionhandler can propagate the exceptionby re-
raisingit,oritcanreturnavaluethatbecomesthevalueofthetryexpression. One
mightinsteaddesignthelanguagetoallowthecomputationtoresumefromthepoint
atwhich theexceptionwas raised. Modifythe interpreterofthissectionto accom-
plishthisbyrunningthebodyofthehandlerwiththecontinuationfromthepointat
whichtheraisewasinvoked.
(cid:3)(cid:3)(cid:3)
Exercise5.40 [ ] Givetheexceptionhandlersinthedefinedlanguagetheability
toeitherreturnorresume.Dothisbypassingthecontinuationfromtheraiseexcep-
tionasasecondargument. Thismayrequireaddingcontinuationsasanewkindof
expressedvalue.Devisesuitablesyntaxforinvokingacontinuationonavalue.
Exercise5.41 [ (cid:3)(cid:3)(cid:3) ] We have shown how to implement exceptions using a data-
structurerepresentationofcontinuations. Wecan’timmediatelyapplytherecipeof
section2.2.3togetaproceduralrepresentation,becausewenowhavetwoobservers:
apply-handlerandapply-cont.Implementthecontinuationsofthissectionasa
pairofprocedures: aone-argumentprocedurerepresentingtheactionofthecontin-
uationunder apply-cont,and azero-argumentprocedurerepresentingits action
underapply-handler.
Exercise5.42 [ (cid:3)(cid:3) ] The preceding exercise captures the continuation only when an
exceptionis raised. Add to the language the ability to capturea continuation any-
wherebyaddingtheformletccIdentifierinExpressionwiththespecification
| (value-of/k   |     | (letcc           | var body) | ρ cont)  |          |
| ------------- | --- | ---------------- | --------- | -------- | -------- |
| = (value-of/k |     | body (extend-env |           | var cont | ρ) cont) |
Such a captured continuation may be invoked with throw: the expressionthrow
Expression to Expression evaluates the two subexpressions. The second expression
should return a continuation, which is applied to the value of the first expression.
Thecurrentcontinuationofthethrowexpressionisignored.
|     | (cid:3)(cid:3) | letcc |     |     |     |
| --- | -------------- | ----- | --- | --- | --- |
Exercise5.43 [ ] Modify as defined in the preceding exercise so that the
capturedcontinuationbecomesanewkindofprocedure,soinsteadofwritingthrow
| exp 1 to | exp 2 ,onewouldwrite(exp |     | 2   | exp 1 ). |     |
| -------- | ------------------------ | --- | --- | -------- | --- |
(cid:3)(cid:3)
Exercise5.44 [ ] An alternative to letcc and throw of the preceding exer-
cises is to add a single procedure to the language. This procedure, which in
call-with-current-continuation,
| Scheme | is called |     |     |     | takes a one-argument |
| ------ | --------- | --- | --- | --- | -------------------- |
procedure, p, and passes to p a procedure that when invoked with one argu-
ment, passes that argument to the current continuation, cont. We could define
call-with-current-continuationintermsofletccandthrowasfollows:
| let | call-with-current-continuation |          |          |            |       |
| --- | ------------------------------ | -------- | -------- | ---------- | ----- |
|     | =                              | proc (p) |          |            |       |
|     |                                | letcc    | cont     |            |       |
|     |                                | in (p    | proc (v) | throw v to | cont) |
| in  | ...                            |          |          |            |       |
Addcall-with-current-continuationtothelanguage.
Thenwriteatransla-
torthattakesthelanguagewithletccandthrowandtranslatesitintothelanguage
withoutletccandthrow,butwithcall-with-current-continuation.

5.5 Threads 179
5.5 Threads
Inmanyprogrammingtasks, one maywishtohave multiple computations
proceedingatonce. When these computations are run in the same address
space as part of the same process, they are usually called threads. In this
section,wewilldiscoverhowtomodifyourinterpretertosimulatetheexe-
cutionofmultithreadedprograms.
Rather than having a single thread of computation, our multithreaded
interpreterwillmaintainseveralthreads.Eachthreadconsistsofacomputa-
tioninprogress,likethoseshownearlierinthischapter. Threadscommuni-
catethroughasinglesharedmemory,usingassignmentasinchapter4.
In our system, the entire computation consists of a pool of threads. Each
threadmaybeeitherrunning,runnable,orblocked. Inoursystem,exactlyone
threadisrunningatatime. Inamulti-CPUsystem,onemighthaveseveral
running threads. The runnable threads will be kept on a queue called the
readyqueue. Theremaybeotherthreadsthatarenotreadytoberun,forone
reasonoranother. Wesaythatthesethreadsareblocked. Blockedthreadswill
beintroducedlaterinthissection.
Threadsarescheduledforexecutionbyascheduler,whichkeepstheready
queueaspartofitsstate. Inaddition,itkeepsatimer,sothatwhenathread
hascompletedacertainnumberofsteps(itstimesliceorquantum),itisinter-
ruptedandputbackonthereadyqueue,andanewthreadisselectedfrom
thereadyqueuetorun. Thisiscalledpre-emptivescheduling.
Our new language is based on IMPLICIT-REFS and is called THREADS.
InTHREADS,newthreadsarecreatedbyaconstructcalledspawn. spawn
takesoneargument,whichshouldevaluatetoaprocedure. Anewthreadis
created,which,whenrun,passesanunspecifiedargumenttothatprocedure.
This thread is not run immediately, but is placed on the readyqueue to be
run when its turn arrives. spawn is executed for effect; in our system we
havearbitrarilydecidedtohaveitreturnthenumber73.
Let’s look at two examples of programs in this language. Figure 5.16
definesaprocedurenoisythattakesalist, printsitsfirstelementandthen
recurson the restof the list. Herethe mainexpressioncreatestwo threads,
whichcompetetoprintoutthelists[1,2,3,4,5]and[6,7,8,9,10].The
exactwayinwhichthelistsareinterleaveddependsonthescheduler;inthis
exampleeachthreadprintsouttwoelementsofitslistbeforethe scheduler
interruptsit.
Figure5.17shows aproduceranda consumer, connected bya bufferini-
tializedto0. Theproducertakesanargumentn,goesaroundthewait loop

| 180 |     |     | 5 Continuation-PassingInterpreters |     |
| --- | --- | --- | ---------------------------------- | --- |
test: two-non-cooperating-threads
letrec
| noisy (l) | = if null?(l) |     |     |     |
| --------- | ------------- | --- | --- | --- |
then 0
|     | else begin | print(car(l)); | (noisy cdr(l)) | end |
| --- | ---------- | -------------- | -------------- | --- |
in
begin
| spawn(proc | (d) (noisy | [1,2,3,4,5]))  | ;   |     |
| ---------- | ---------- | -------------- | --- | --- |
| spawn(proc | (d) (noisy | [6,7,8,9,10])) | ;   |     |
print(100);
33
end
100
1
2
6
7
3
4
8
9
5
10
| correct outcome: | 33               |     |     |     |
| ---------------- | ---------------- | --- | --- | --- |
| actual outcome:  | #(struct:num-val |     | 33) |     |
correct
| Figure5.16 | Twothreadsshowinginterleavedcomputation |     |     |     |
| ---------- | --------------------------------------- | --- | --- | --- |
5 times, and then puts n in the buffer. Each time through the wait loop,
it prints the countdown timer (expressed in 200s). The consumer takes an
argument (which it ignores) and goes into a loop, waiting for the buffer to
becomenon-zero.Eachtimethroughthisloop,itprintsacounter(expressed
in100s)toshowhowlongithaswaitedforitsresult. Themainthreadputs
theproduceronthereadyqueue,prints300,andstartstheconsumerinthe
main thread. So the first two items, 300 and 205, are printed by the main
thread.Asintheprecedingexample,theconsumerthreadandtheproducer
threadeachgoaroundtheirloopabouttwicebeforebeinginterrupted.

5.5 Threads 181
| let buffer | = 0        |          |     |
| ---------- | ---------- | -------- | --- |
| in let     | producer = | proc (n) |     |
letrec
|     | wait(k) = | if zero?(k)     |     |
| --- | --------- | --------------- | --- |
|     |           | then set buffer | = n |
else begin
print(-(k,-200));
|     |     | (wait | -(k,1)) |
| --- | --- | ----- | ------- |
end
in (wait 5)
| in let | consumer | = proc (d)   |                    |
| ------ | -------- | ------------ | ------------------ |
|        | letrec   | busywait (k) | = if zero?(buffer) |
then begin
print(-(k,-100));
(busywait -(k,-1))
end
else buffer
|     | in (busywait | 0)            |       |
| --- | ------------ | ------------- | ----- |
| in  | begin        |               |       |
|     | spawn(proc   | (d) (producer | 44)); |
print(300);
|     | (consumer | 86) |     |
| --- | --------- | --- | --- |
end
300
205
100
101
204
203
102
103
202
201
104
105
| correct | outcome: 44               |     |     |
| ------- | ------------------------- | --- | --- |
| actual  | outcome: #(struct:num-val |     | 44) |
correct
| Figure5.17 | Aproducerandconsumer,linkedbyabuffer |     |     |
| ---------- | ------------------------------------ | --- | --- |

182 5 Continuation-PassingInterpreters
Theimplementationstartswithacontinuation-passinginterpreterforthe
language IMPLICIT-REFS. This is similar to the one in section 5.1, with
the addition of a store like the one in IMPLICIT-REFS (of course!) and a
set-rhs-contcontinuationbuilderliketheoneinexercise5.9.
Tothisinterpreterweaddascheduler. Theschedulerkeepsastateconsist-
ingoffourvaluesandprovidessixproceduresinitsinterfaceformanipulat-
ingthosevalues. Theseareshowninfigure5.18.
Figure 5.19 shows the implementation of this interface. Here (enqueue
q val) returns a queue like q, except that val has been placed at the end.
(dequeue q f) takesthe head of the queue and the restof the queue and
passesthemtof asarguments.
WerepresentathreadasaSchemeprocedureofnoargumentsthatreturns
anexpressedvalue:
Thread = () → ExpVal
procedurerun-next-thread
| Ifthe readyqueueisnon-empty, | thenthe |     |
| ---------------------------- | ------- | --- |
takesthefirstthreadfromthereadyqueueandrunsit,givingitanewtime
sliceofsizethe-max-time-slice.Italsosetsthe-ready-queuesothat
itconsistsoftheremainingthreads,ifany. Ifthereadyqueueisempty,then
run-next-thread returns the contents of the-final-answer. This is
howthecomputationeventuallyterminates.
Wenextturntotheinterpreter.Aspawnexpressionevaluatesitsargument
in a continuation which, when executed, places a new thread on the ready
queue and continues by returning 73 to the caller of the spawn. The new
thread,whenexecuted,passesanarbitraryvalue(here28)totheprocedure
thatwasthe valueofthespawn’sargument. Toaccomplishthis, weaddto
value-of/ktheclause
| (spawn-exp (exp) |         |     |
| ---------------- | ------- | --- |
| (value-of/k      | exp env |     |
| (spawn-cont      | cont))) |     |
andtoapply-conttheclause
| (spawn-cont  | (saved-cont)  |        |
| ------------ | ------------- | ------ |
| (let ((proc1 | (expval->proc | val))) |
(place-on-ready-queue!
| (lambda            | ()  |       |
| ------------------ | --- | ----- |
| (apply-procedure/k |     | proc1 |
(num-val 28)
(end-subthread-cont))))
| (apply-cont | saved-cont | (num-val 73)))) |
| ----------- | ---------- | --------------- |

5.5 Threads 183
InternalStateoftheScheduler
the-ready-queue thereadyqueue
the-final-answer thevalueofthemainthread,ifdone
the-max-time-slice thenumberofstepsthateachthreadmayrun
the-time-remaining thenumberofstepsremainingforthe
currentlyrunningthread.
SchedulerInterface
→
initialize-scheduler! : Int Unspecified
initializestheschedulerstate
→
place-on-ready-queue! : Thread Unspecified
placesthreadonthereadyqueue
→
run-next-thread : () FinalAnswer
runsnextthread.Ifnoreadythreads,returnsthe
finalanswer.
→
set-final-answer! : ExpVal Unspecified
setsthefinalanswer
→
time-expired? : () Bool
testswhethertimeris0
→
decrement-timer! : () Unspecified
decrementstime-remaining
Figure5.18 Stateandinterfaceofthescheduler

184 5 Continuation-PassingInterpreters
| initialize-scheduler!         | : Int              | → Unspecified         |     |
| ----------------------------- | ------------------ | --------------------- | --- |
| (define initialize-scheduler! |                    |                       |     |
| (lambda                       | (ticks)            |                       |     |
| (set!                         | the-ready-queue    | (empty-queue))        |     |
| (set!                         | the-final-answer   | ’uninitialized)       |     |
| (set!                         | the-max-time-slice | ticks)                |     |
| (set!                         | the-time-remaining | the-max-time-slice))) |     |
| place-on-ready-queue!         | : Thread           | → Unspecified         |     |
| (define place-on-ready-queue! |                    |                       |     |
| (lambda                       | (th)               |                       |     |
| (set!                         | the-ready-queue    |                       |     |
| (enqueue                      | the-ready-queue    | th))))                |     |
| run-next-thread               | : () → FinalAnswer |                       |     |
| (define run-next-thread       |                    |                       |     |
| (lambda                       | ()                 |                       |     |
| (if (empty?                   | the-ready-queue)   |                       |     |
the-final-answer
| (dequeue | the-ready-queue |     |     |
| -------- | --------------- | --- | --- |
(lambda (first-ready-thread other-ready-threads)
|     | (set! the-ready-queue | other-ready-threads) |     |
| --- | --------------------- | -------------------- | --- |
(set! the-time-remaining the-max-time-slice)
(first-ready-thread))))))
| set-final-answer!         | : ExpVal         | → Unspecified |     |
| ------------------------- | ---------------- | ------------- | --- |
| (define set-final-answer! |                  |               |     |
| (lambda                   | (val)            |               |     |
| (set!                     | the-final-answer | val)))        |     |
→
| time-expired?         | : () Bool             |     |     |
| --------------------- | --------------------- | --- | --- |
| (define time-expired? |                       |     |     |
| (lambda               | ()                    |     |     |
| (zero?                | the-time-remaining))) |     |     |
→
| decrement-timer!         | : ()               | Unspecified           |       |
| ------------------------ | ------------------ | --------------------- | ----- |
| (define decrement-timer! |                    |                       |       |
| (lambda                  | ()                 |                       |       |
| (set!                    | the-time-remaining | (- the-time-remaining | 1)))) |
|                          | Figure5.19         | Thescheduler          |       |

5.5 Threads 185
This is what the trampolined interpreter did when it created a snapshot:
| itpackagedupacomputation(here(lambda |     |     | () (apply-procedure/k |     |
| ------------------------------------ | --- | --- | --------------------- | --- |
...))) and passed it to another procedure for processing. In the trampo-
line example, we passed the thread to the trampoline, which simply ran it.
Here we place the new thread on the ready queue and continue our own
computation.
Thisleadsustothe keyquestion: whatcontinuation should we runeach
threadin?
• Themainthreadshouldberunwithacontinuationthatrecordsthevalue
ofthemainthreadasthefinalanswer,andthenrunsanyremainingready
threads.
• Whenthesubthreadfinishes,thereisnowaytoreportitsvalue,sowerun
itinacontinuationthatignoresthevalueandsimplyrunsanyremaining
readythreads.
Thisgivesustwonewcontinuations, whose behaviorisimplementedby
thefollowinglinesinapply-cont:
| (end-main-thread-cont |                    | ()   |     |     |
| --------------------- | ------------------ | ---- | --- | --- |
|                       | (set-final-answer! | val) |     |     |
(run-next-thread))
| (end-subthread-cont |     | ()  |     |     |
| ------------------- | --- | --- | --- | --- |
(run-next-thread))
Westarttheentiresystemwithvalue-of-program:
× →
| value-of-program | : Int            | Program | FinalAnswer |     |
| ---------------- | ---------------- | ------- | ----------- | --- |
| (define          | value-of-program |         |             |     |
| (lambda          | (timeslice       | pgm)    |             |     |
(initialize-store!)
| (initialize-scheduler! |         | timeslice) |     |     |
| ---------------------- | ------- | ---------- | --- | --- |
| (cases                 | program | pgm        |     |     |
(a-program (exp1)
(value-of/k
exp1
(init-env)
(end-main-thread-cont))))))
Last,wemodifyapply-conttodecrementthetimereachtimeitiscalled.
Ifthetimerhasexpired,thenthecurrentcomputationissuspended. Wedo
apply-cont
| this by putting | on the ready | queue a | thread that will | try the |
| --------------- | ------------ | ------- | ---------------- | ------- |
again,withthetimerrestoredbysomecalltorun-next-thread.

186 5 Continuation-PassingInterpreters
| let | x = 0      |             |     |
| --- | ---------- | ----------- | --- |
| in  | let incr_x | = proc (id) |     |
proc (dummy)
|     |     | set x = | -(x,-1) |
| --- | --- | ------- | ------- |
in begin
|     | spawn((incr_x | 100)); |     |
| --- | ------------- | ------ | --- |
|     | spawn((incr_x | 200)); |     |
|     | spawn((incr_x | 300))  |     |
end
|            |                 | Figure5.20 Anunsafecounter |     |
| ---------- | --------------- | -------------------------- | --- |
| apply-cont | : Cont ×        | ExpVal → FinalAnswer       |     |
| (define    | apply-cont      |                            |     |
| (lambda    | (cont val)      |                            |     |
| (if        | (time-expired?) |                            |     |
(begin
(place-on-ready-queue!
|     | (lambda | () (apply-cont | cont val))) |
| --- | ------- | -------------- | ----------- |
(run-next-thread))
(begin
(decrement-timer!)
|     | (cases continuation | cont |     |
| --- | ------------------- | ---- | --- |
...)))))
Sharedvariablesareanunreliablemethodofcommunicationbecausesev-
eralthreadsmaytrytowritetothesamevariable.Forexample,considerthe
programinfigure5.20. Herewecreatethreethreads,eachofwhichtriesto
incrementthe same counter x. If one threadreadsthe counter, but is inter-
ruptedbeforeitcanupdateit, then boththreadswill change the counter to
thesamenumber. Hencethecountermaybecome2,oreven1,ratherthan3.
Wewouldliketobeabletoensurethatinterferenceslikethisdonotoccur.
Similarly,wewouldliketobeabletoorganizeourprogramsothatthecon-
sumerinfigure5.17doesn’thavetobusy-wait. Instead,itshouldbeableto
putitselftosleepandbeawakenedwhentheproducerhasinsertedavalue
inthesharedbuffer.

5.5 Threads 187
Therearemanywaysto designsucha synchronization facility. A simple
oneisthemutex(shortformutualexclusion)orbinarysemaphore.
A mutex may either be openor closed. It also contains a queue of threads
thatarewaitingforthemutextobecomeopen. Therearethreeoperationson
mutexes:
• mutex is an operation that takes no arguments and creates an initially
openmutex.
• waitisaunaryoperationbywhichathreadindicatesthatitwantsaccess
to a mutex. Itsargumentmust be a mutex. Its behavior dependson the
stateofthemutex.
– Ifthemutexisclosed,thenthecurrentthreadisplacedonthemutex’s
waitqueue,andissuspended.Wesaythatthecurrentthreadisblocked
waitingforthismutex.
– Ifthemutexisopen,itbecomesclosedandthecurrentthreadcontinues
torun.
Awaitisexecutedforeffectonly;itsreturnvalueisunspecified.
• signalisaunaryoperationbywhich athreadindicatesthatitisready
toreleaseamutex. Itsargumentmustbeamutex.
– If the mutex is closed, and there are no threads waiting on its wait
queue,thenmutexbecomesopenandthecurrentthreadproceeds.
– If the mutex is closed and there are threads in its wait queue, then
oneofthethreadsfromthewaitqueueisputonthescheduler’sready
queue, and the mutex remains closed. The thread that executed the
signalcontinuestocompute.
– Ifthemutexisopen,thenthethreadleavesitopenandproceeds.
A signal is executed for effect only; its return value is unspecified. A
signal operation always succeeds: the thread that executes it remains
therunningthread.
These properties guarantee that only one thread can execute between a
successive pair of calls to wait and signal. This portion of the program
iscalledacriticalregion. Itisimpossible fortwodifferentthreadstobecon-
currentlyexcecutingcodeinacriticalregion. Forexample,figure5.21shows

188 5 Continuation-PassingInterpreters
let x = 0
| in let mut =  | mutex()     |     |
| ------------- | ----------- | --- |
| in let incr_x | = proc (id) |     |
proc (dummy)
begin
wait(mut);
|     | set x = -(x,-1); |     |
| --- | ---------------- | --- |
signal(mut)
end
in begin
| spawn((incr_x | 100)); |     |
| ------------- | ------ | --- |
| spawn((incr_x | 200)); |     |
| spawn((incr_x | 300))  |     |
end
| Figure5.21 | Asafecounterusingamutex |     |
| ---------- | ----------------------- | --- |
ourpreviousexample,withamutexinsertedaroundthecriticalline. Inthis
program,onlyone threadcanexecutethe set x = -(x,-1)atatime, so
thecounterisguaranteedtoreachthefinalvalueof3.
Wemodelamutexastworeferences:onetoitsstate(eitheropenorclosed)
and one to a list of threads waiting for this mutex. We also make mutexes
expressedvalues.
(define-datatype mutex mutex?
(a-mutex
| (ref-to-closed?    | reference?)   |     |
| ------------------ | ------------- | --- |
| (ref-to-wait-queue | reference?))) |     |
Weaddtheappropriatelinetovalue-of/k
| (mutex-exp  | ()              |                |
| ----------- | --------------- | -------------- |
| (apply-cont | cont (mutex-val | (new-mutex)))) |
where
| new-mutex : () → | Mutex |     |
| ---------------- | ----- | --- |
(define new-mutex
(lambda ()
(a-mutex
(newref #f)
(newref ’()))))

5.5 Threads 189
wait and signal will be new unary operations, which simply call the
procedures wait-for-mutex and signal-mutex. wait and signal
bothevaluatetheirsingleargument,soinapply-contwewrite
(wait-cont (saved-cont)
(wait-for-mutex
(expval->mutex val)
(lambda () (apply-cont saved-cont (num-val 52)))))
(signal-cont (saved-cont)
(signal-mutex
(expval->mutex val)
(lambda () (apply-cont saved-cont (num-val 53)))))
Nowwecanwritewait-for-mutexandsignal-mutex. Theseproce-
durestaketwoarguments:amutexandathread,andtheyworkasdescribed
inthetextabove(figure5.22).
(cid:3)
Exercise5.45 [ ]Addtothelanguageofthissectionaconstructcalledyield.When-
everathreadexecutesayield,itisplacedonthereadyqueue,andthethreadatthe
headofthereadyqueueisrun. Whenthethreadisresumed,itshouldappearasif
thecalltoyieldhadreturnedthenumber99.
(cid:3)(cid:3)
Exercise5.46 [ ]Inthesystemofexercise5.45,athreadmaybeplacedontheready
queueeitherbecauseitstimeslothasbeenexhaustedorbecauseitchosetoyield. In
the latter case, it willbe restartedwitha fulltime slice. Modifythe systemso that
thereadyqueuekeepstrackoftheremainingtimeslice(ifany)ofeachthread,and
restartsthethreadonlywiththetimeithasremaining.
(cid:3)
Exercise5.47 [ ] Whathappensifweareleftwithtwosubthreads,eachwaitingfor
amutexheldbytheothersubthread?
(cid:3)
Exercise5.48 [ ] Wehaveusedaproceduralrepresentationofthreads. Replacethis
byadata-structurerepresentation.
(cid:3)
Exercise5.49 [ ] Doexercise5.15(continuationsasastackofframes)forTHREADS.
(cid:3)(cid:3)
Exercise5.50 [ ] Registerizetheinterpreterofthissection.Whatisthesetofmutu-
allytail-recursiveproceduresthatmustberegisterized?
(cid:3)(cid:3)(cid:3)
Exercise5.51 [ ]Wewouldliketobeabletoorganizeourprogramsothatthecon-
sumerinfigure5.17doesn’thavetobusy-wait.Instead,itshouldbeabletoputitself
to sleepand be awakened when the producerhas put avalue inthe buffer. Either
writeaprogramwithmutexestodothis, orimplementasynchronizationoperator
thatmakesthispossible.
(cid:3)(cid:3)(cid:3)
Exercise5.52 [ ] Write a program using mutexes that will be like the program
in figure 5.21, except that the main thread waits for all three of the subthreads to
terminate,andthenreturnsthevalueofx.

190 5 Continuation-PassingInterpreters
× →
| wait-for-mutex | : Mutex         | Thread             | FinalAnswer |     |
| -------------- | --------------- | ------------------ | ----------- | --- |
| usage: waits   | for mutex       | to be open,        | then closes | it. |
| (define        | wait-for-mutex  |                    |             |     |
| (lambda        | (m th)          |                    |             |     |
| (cases         | mutex m         |                    |             |     |
| (a-mutex       | (ref-to-closed? | ref-to-wait-queue) |             |     |
(cond
((deref ref-to-closed?)
|     | (setref! | ref-to-wait-queue         |     |      |
| --- | -------- | ------------------------- | --- | ---- |
|     | (enqueue | (deref ref-to-wait-queue) |     | th)) |
(run-next-thread))
(else
|     | (setref! | ref-to-closed? | #t) |     |
| --- | -------- | -------------- | --- | --- |
(th)))))))
× →
| signal-mutex | : Mutex         | Thread FinalAnswer      |                      |     |
| ------------ | --------------- | ----------------------- | -------------------- | --- |
| (define      | signal-mutex    |                         |                      |     |
| (lambda      | (m th)          |                         |                      |     |
| (cases       | mutex m         |                         |                      |     |
| (a-mutex     | (ref-to-closed? | ref-to-wait-queue)      |                      |     |
|              | (let ((closed?  | (deref ref-to-closed?)) |                      |     |
|              | (wait-queue     | (deref                  | ref-to-wait-queue))) |     |
(if closed?
|     | (if (empty? | wait-queue)       |                    |     |
| --- | ----------- | ----------------- | ------------------ | --- |
|     | (setref!    | ref-to-closed?    | #f)                |     |
|     | (dequeue    | wait-queue        |                    |     |
|     | (lambda     | (first-waiting-th | other-waiting-ths) |     |
(place-on-ready-queue!
first-waiting-th)
(setref!
ref-to-wait-queue
other-waiting-ths)))))
(th))))))
|     | Figure5.22 | wait-for-mutexandsignal-mutex |     |     |
| --- | ---------- | ----------------------------- | --- | --- |

5.5 Threads 191
(cid:3)(cid:3)(cid:3)
Exercise5.53 [ ] Modifythethreadpackagetoincludethreadidentifiers.Eachnew
threadisassociatedwithafreshthreadidentifier.Whenthechildthreadisspawned,
it is passed its thread identifier as a value, rather than the arbitrary value 28 used
inthissection. Thechild’snumberisalsoreturnedtotheparentasthevalueofthe
spawnexpression. Instrumenttheinterpretertotracethe creationofthreadidenti-
fiers. Checktoseethatthereadyqueuecontainsatmostonethreadforeachthread
identifier.Howcanachildthreadknowitsparent’sidentifier?Whatshouldbedone
aboutthethreadidentifieroftheoriginalprogram?
(cid:3)(cid:3)
Exercise5.54 [ ] Addtotheinterpreterofexercise5.53akillfacility. Thekill
construct,whengivenathreadnumber,findsthecorrespondingthreadontheready
queueoranyofthewaitingqueuesandremovesit. Inaddition,killshouldreturn
atruevalueifthetargetthreadisfoundandfalseifthethreadnumberisnotfound
onanyqueue.
(cid:3)(cid:3)
Exercise5.55 [ ] Addtotheinterpreterofexercise5.53aninterthreadcommunica-
tionfacility,inwhicheachthreadcansendavaluetoanotherthreadusingitsthread
identifier.Athreadcanreceivemessageswhenitchooses,blockingifnomessagehas
beensenttoit.
(cid:3)(cid:3)
Exercise5.56 [ ] Modifytheinterpreterofexercise5.55sothatratherthansharinga
store,eachthreadhasitsownstore.Insuchalanguage,mutexescanalmostalwaysbe
avoided.Rewritetheexampleofthissectioninthislanguage,withoutusingmutexes.
(cid:3)(cid:3)(cid:3)
Exercise5.57 [ ] Therearelotsofdifferentsynchronizationmechanismsinyour
favoriteOSbook.Pickthreeandimplementtheminthisframework.
(cid:3)
Exercise5.58 [definitely ] Gooffwithyourfriendsandhavesomepizza,butmake
sureonlyonepersonatatimegrabsapiece!

6
| Continuation-Passing |     |     | Style |     |
| -------------------- | --- | --- | ----- | --- |
In chapter 5, we took an interpreter and rewrote it so that all of the major
procedurecallsweretailcalls.Bydoingso,weguaranteedthattheinterpreter
usesatmostaboundedamountofcontrolcontextatanyonetime,nomatter
howlargeorcomplexaprogramitiscalledupontointerpret. Thisproperty
iscallediterativecontrolbehavior.
We achieved this goal by passing an extra parameter, the continuation, to
eachprocedure.Thisstyleofprogrammingiscalledcontinuation-passingstyle
orCPS,anditisnotrestrictedtointerpreters.
Inthischapterwedevelopasystematicmethodfortransforminganypro-
cedureintoanequivalentprocedurewhosecontrolbehaviorisiterative.This
isaccomplishedbyconvertingitintocontinuation-passingstyle.
| 6.1 WritingProgramsin |     | Continuation-Passing |     | Style |
| --------------------- | --- | -------------------- | --- | ----- |
We canuse CPS for other things besides interpreters. Let’s consider an old
favorite,thefactorialprogram:
(define fact
(lambda (n)
|     | (if (zero? | n) 1 (* | n (fact (- | n 1)))))) |
| --- | ---------- | ------- | ---------- | --------- |
Acontinuation-passingversionoffactorialwouldlooksomethinglike
(define fact
(lambda (n)
|     | (fact/k | n (end-cont)))) |     |     |
| --- | ------- | --------------- | --- | --- |
(define fact/k
|     | (lambda (n  | cont)   |             |             |
| --- | ----------- | ------- | ----------- | ----------- |
|     | (if (zero?  | n)      |             |             |
|     | (apply-cont | cont    | 1)          |             |
|     | (fact/k     | (- n 1) | (fact1-cont | n cont))))) |

194 6 Continuation-PassingStyle
where
| (apply-cont   |     | (end-cont)  |         | val)    | = val      |     |
| ------------- | --- | ----------- | ------- | ------- | ---------- | --- |
| (apply-cont   |     | (fact1-cont |         | n       | cont) val) |     |
| = (apply-cont |     |             | cont (* | n val)) |            |     |
Inthisversion,allthecallstofact/kandapply-contareintailposition
andthereforebuildupnocontrolcontext.
Wecanimplementthesecontinuationsasdatastructuresbywriting
| (define-datatype |     |     | continuation |     | continuation? |     |
| ---------------- | --- | --- | ------------ | --- | ------------- | --- |
(end-cont)
(fact1-cont
(n integer?)
|         | (cont       | continuation?))) |          |            |             |           |
| ------- | ----------- | ---------------- | -------- | ---------- | ----------- | --------- |
| (define | apply-cont  |                  |          |            |             |           |
| (lambda |             | (cont            | val)     |            |             |           |
|         | (cases      | continuation     |          | cont       |             |           |
|         | (end-cont   |                  | () val)  |            |             |           |
|         | (fact1-cont |                  | (saved-n |            | saved-cont) |           |
|         | (apply-cont |                  |          | saved-cont | (* saved-n  | val)))))) |
We can transform this program in many ways. We could, for example,
registerizeit,asshowninfigure6.1.
We could eventrampoline this version, asshown in figure 6.2. If we did
this in an ordinary imperative language, we would of course replace the
trampolinebyaproperloop.
However,ourprimaryconcerninthischapterwillbewhathappenswhen
weuseaproceduralrepresentation,aswedidinfigure5.2. Recallthatinthe
proceduralrepresentation,acontinuationisrepresentedbyitsactionunder
apply-cont.Theproceduralrepresentationlookslike
| (define | end-cont    |                |            |     |               |     |
| ------- | ----------- | -------------- | ---------- | --- | ------------- | --- |
| (lambda |             | ()             |            |     |               |     |
|         | (lambda     | (val)          | val)))     |     |               |     |
| (define | fact1-cont  |                |            |     |               |     |
| (lambda |             | (n saved-cont) |            |     |               |     |
|         | (lambda     | (val)          |            |     |               |     |
|         | (apply-cont |                | saved-cont |     | (* n val))))) |     |
| (define | apply-cont  |                |            |     |               |     |
| (lambda |             | (cont          | val)       |     |               |     |
|         | (cont       | val)))         |            |     |               |     |

6.1 WritingProgramsinContinuation-PassingStyle 195
(define n ’uninitialized)
(define cont ’uninitialized)
(define val ’uninitialized)
(define fact
(lambda (arg-n)
(set! cont (end-cont))
(set! n arg-n)
(fact/k)))
(define fact/k
(lambda ()
(if (zero? n)
(begin
| (set! val | 1)  |     |
| --------- | --- | --- |
(apply-cont))
(begin
| (set! cont | (fact1-cont | n cont)) |
| ---------- | ----------- | -------- |
| (set! n    | (- n 1))    |          |
(fact/k)))))
(define apply-cont
(lambda ()
| (cases continuation | cont                 |     |
| ------------------- | -------------------- | --- |
| (end-cont           | () val)              |     |
| (fact1-cont         | (saved-n saved-cont) |     |
| (set! cont          | saved-cont)          |     |
| (set! n             | saved-n)             |     |
(apply-cont)))))
fact/kregisterized
Figure6.1
Wecandoevenbetterbytakingeachcalltoacontinuation-builderinthe
programandreplacingitbyitsdefinition. Thistransformationiscalledinlin-
ing, becausethe definitions areexpandedin-line. We alsoinline thecallsto
apply-cont, so instead of writing (apply-cont cont val), we’ll just
write(cont val).

196 6 Continuation-PassingStyle
(define n ’uninitialized)
(define cont ’uninitialized)
(define val ’uninitialized)
(define pc ’uninitialized)
(define fact
(lambda (arg-n)
(set! cont (end-cont))
(set! n arg-n)
(set! pc fact/k)
(trampoline!)
val))
(define trampoline!
(lambda ()
(if pc
(begin
(pc)
(trampoline!)))))
(define fact/k
(lambda ()
(if (zero? n)
(begin
| (set! val | 1)           |     |
| --------- | ------------ | --- |
| (set! pc  | apply-cont)) |     |
(begin
| (set! cont | (fact1-cont | n cont)) |
| ---------- | ----------- | -------- |
| (set! n    | (- n 1))    |          |
| (set! pc   | fact/k))))) |          |
(define apply-cont
(lambda ()
| (cases continuation | cont                             |     |
| ------------------- | -------------------------------- | --- |
| (end-cont           | ()                               |     |
| (set! pc            | #f))                             |     |
| (fact1-cont         | (saved-n saved-cont)             |     |
| (set! cont          | saved-cont)                      |     |
| (set! n             | saved-n)                         |     |
| (set! pc            | apply-cont)))))                  |     |
| Figure6.2           | fact/kregisterizedandtrampolined |     |

| 6.1 | WritingProgramsinContinuation-PassingStyle |     |     |     |     |     |     | 197 |
| --- | ------------------------------------------ | --- | --- | --- | --- | --- | --- | --- |
Ifweinlinealltheusesofcontinuationsinthisway,weget
| (define |         | fact     |                 |     |         |       |                 |     |
| ------- | ------- | -------- | --------------- | --- | ------- | ----- | --------------- | --- |
|         | (lambda | (n)      |                 |     |         |       |                 |     |
|         | (fact/k | n        | (lambda (val)   |     | val)))) |       |                 |     |
| (define |         | fact/k   |                 |     |         |       |                 |     |
|         | (lambda | (n cont) |                 |     |         |       |                 |     |
|         | (if     | (zero?   | n)              |     |         |       |                 |     |
|         | (cont   | 1)       |                 |     |         |       |                 |     |
|         | (fact/k |          | (- n 1) (lambda |     | (val)   | (cont | (* n val))))))) |     |
Wecanreadthedefinitionoffact/kas:
|     | Ifniszero,send1tothecontinuation.Otherwise,evaluatefactofn |     |     |     |     |     |     | − 1 |
| --- | ---------------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
inacontinuationthatcallstheresultval,andthensendstothecontinuation
|                                             | thevalue(* | n val). |     |     |     |     |        |         |
| ------------------------------------------- | ---------- | ------- | --- | --- | --- | --- | ------ | ------- |
| Theprocedurefact/khasthepropertythat(fact/k |            |         |     |     |     |     | n g) = | (g n!). |
=
This is easy to show by induction on n. For the base step, when n 0, we
calculate
| (fact/k |     | 0 g) = | (g 1) = | (g (fact | 0)) |     |     |     |
| ------- | --- | ------ | ------- | -------- | --- | --- | --- | --- |
Fortheinductionstep,weassumethat(fact/k n g) = (g n!),forsome
| valueofnandtrytoshowthat(fact/k |     |     |     |     |     | +   | g) = (g + 1)!). |      |
| ------------------------------- | --- | --- | --- | --- | --- | --- | --------------- | ---- |
|                                 |     |     |     |     | (n  | 1)  | (n              | Todo |
this,wecalculate:
| (fact/k |         | n+1 g)  |       |     |        |         |     |     |
| ------- | ------- | ------- | ----- | --- | ------ | ------- | --- | --- |
| =       | (fact/k | (lambda | (val) | (g  | (* n+1 | val)))) |     |     |
n
n+1
| =   | ((lambda | (val) | (g (* | val))) |     | (bytheinductionhypothesis) |     |     |
| --- | -------- | ----- | ----- | ------ | --- | -------------------------- | --- | --- |
|     | (fact    | n))   |       |        |     |                            |     |     |
n+1
| =   | (g (*    | (fact | n))) |     |     |     |     |     |
| --- | -------- | ----- | ---- | --- | --- | --- | --- | --- |
| =   | (g (fact | n+1)) |      |     |     |     |     |     |
Thiscompletestheinduction.
Herethe gappearsasacontextargument,asinsection1.3,andtheprop-
ertythat(fact/k n g) = (g n!)servesastheindependentspecification,
followingourprincipleofNoMysteriousAuxiliaries.

198 6 Continuation-PassingStyle
Nowlet’sdothesamethingfortheFibonaccisequencefib.Westartwith
(define fib
(lambda (n)
(if (< n 2)
1
(+
(fib (- n 1))
(fib (- n 2))))))
Here we have two recursive calls to fib, so we will need an end-cont
and two continuation-builders, one for each argument, just as we did for
differenceexpressionsinsection5.1.
(define fib
(lambda (n)
(fib/k n (end-cont))))
(define fib/k
(lambda (n cont)
(if (< n 2)
(apply-cont cont 1)
(fib/k (- n 1) (fib1-cont n cont)))))
(apply-cont (end-cont) val) = val
(apply-cont (fib1-cont n cont) val1)
= (fib/k (- n 2) (fib2-cont val1 cont))
(apply-cont (fib2-cont val1 cont) val2)
= (apply-cont cont (+ val1 val2))
Intheproceduralrepresentationwehave
(define end-cont
(lambda ()
(lambda (val) val)))
(define fib1-cont
(lambda (n cont)
(lambda (val1)
(fib/k (- n 2) (fib2-cont val1 cont)))))
(define fib2-cont
(lambda (val1 cont)
(lambda (val2)
(apply-cont cont (+ val1 val2)))))
(define apply-cont
(lambda (cont val)
(cont val)))

| 6.1 WritingProgramsinContinuation-PassingStyle |     |     |     |     | 199 |
| ---------------------------------------------- | --- | --- | --- | --- | --- |
Ifweinlinealltheusesoftheseprocedures,weget
| (define fib   |                 |         |     |     |     |
| ------------- | --------------- | ------- | --- | --- | --- |
| (lambda       | (n)             |         |     |     |     |
| (fib/k        | n (lambda (val) | val)))) |     |     |     |
| (define fib/k |                 |         |     |     |     |
| (lambda       | (n cont)        |         |     |     |     |
| (if (<        | n 2)            |         |     |     |     |
| (cont         | 1)              |         |     |     |     |
| (fib/k        | (- n 1)         |         |     |     |     |
(lambda (val1)
|     | (fib/k (- n 2) |     |     |     |     |
| --- | -------------- | --- | --- | --- | --- |
(lambda (val2)
|     | (cont (+ | val1 val2))))))))) |     |     |     |
| --- | -------- | ------------------ | --- | --- | --- |
Aswedidforfactorial,wecanreadthisdefinitionas
| If n < 2, | send 1 to the continuation. |     | Otherwise, work | on n − 1 | in a |
| --------- | --------------------------- | --- | --------------- | -------- | ---- |
−
| continuationthatcallstheresultval1andthenworksonn |            |          |         | 2inacontin- |        |
| ------------------------------------------------- | ---------- | -------- | ------- | ----------- | ------ |
|                                                   | val2       |          | (+ val1 | val2)       |        |
| uation that calls                                 | the result | and then | sends   |             | to the |
continuation.
It is easy to see, by the same reasoning we used for fact, that for any
g, (fib/k n g) = (g(fib n)). Here is an artificialexample that extends
theseideas.
| (lambda (x) |     |     |     |     |     |
| ----------- | --- | --- | --- | --- | --- |
(cond
| ((zero? | x) 17)           |               |     |     |     |
| ------- | ---------------- | ------------- | --- | --- | --- |
| ((= x   | 1) (f x))        |               |     |     |     |
| ((= x   | 2) (+ 22 (f x))) |               |     |     |     |
| ((= x   | 3) (g 22 (f x))) |               |     |     |     |
| ((= x   | 4) (+ (f x) 33   | (g y)))       |     |     |     |
| (else   | (h (f x) (- 44   | y) (g y)))))) |     |     |     |
becomes
| (lambda (x | cont) |     |     |     |     |
| ---------- | ----- | --- | --- | --- | --- |
(cond
| ((zero?                        | x) (cont 17))   |                               |                    |     |     |
| ------------------------------ | --------------- | ----------------------------- | ------------------ | --- | --- |
| ((= x                          | 1) (f x cont))  |                               |                    |     |     |
| ((= x                          | 2) (f x (lambda | (v1) (cont                    | (+ 22 v1)))))      |     |     |
| ((= x                          | 3) (f x (lambda | (v1) (g                       | 22 v1 cont))))     |     |     |
| ((= x                          | 4) (f x (lambda | (v1)                          |                    |     |     |
|                                | (g              | y (lambda (v2)                |                    |     |     |
|                                |                 | (cont (+                      | v1 33 v2))))))     |     |     |
| (else                          | (f x (lambda    | (v1)                          |                    |     |     |
|                                | (g y (lambda    | (v2)                          |                    |     |     |
|                                |                 | (h v1 (- 44                   | y) v2 cont)))))))) |     |     |
| wheretheproceduresf,g,h,j,andp |                 | havebeensimilarlytransformed. |                    |     |     |

200 6 Continuation-PassingStyle
| • Inthe(zero? | x)line,wereturn17tothecontinuation.              |     |     |     |     |     |
| ------------- | ------------------------------------------------ | --- | --- | --- | --- | --- |
| • Inthe(=     | x 1)line,wecallftail-recursively.                |     |     |     |     |     |
| • Inthe(=     | x 2)line,wecallfinanoperandpositionofanaddition. |     |     |     |     |     |
• Inthe(= x 3)line,wecallfinanoperandpositionofaprocedurecall.
• Inthe(= x 4)line,wehavetwoprocedurecallsinoperandpositionsin
anaddition.
• Intheelseline,wehavetwoprocedurecallsinoperandpositioninside
anotherprocedurecall.
Fromtheseexamples,wecanseeapatternemerging.
TheCPSRecipe
| Toconvertaprogram | tocontinuation-passingstyle |     |     |     |     |     |
| ----------------- | --------------------------- | --- | --- | --- | --- | --- |
1. Passeachprocedureanextraparameter(typicallycontork).
| 2. Whenever                                    | the procedure | returns     | a constant | or             | variable, | return that     |
| ---------------------------------------------- | ------------- | ----------- | ---------- | -------------- | --------- | --------------- |
| valuetothecontinuationinstead,aswedidwith(cont |               |             |            |                |           | 7)above.        |
| 3. Whenever                                    | a procedure   | call occurs | in a       | tail position, |           | call the proce- |
durewiththesamecontinuationcont.
| 4. Whenever   | a procedure | call occurs           | in an | operand | position, | evaluate      |
| ------------- | ----------- | --------------------- | ----- | ------- | --------- | ------------- |
| the procedure | call        | in a new continuation |       | that    | gives     | a name to the |
resultandcontinueswiththecomputation.
Theserulesareinformal,buttheyillustratethepatterns.
(cid:3)
Exercise6.1 [ ]Considerfigure6.2without(set! pc fact/k)inthedefinitionof
fact/k and without (set! pc apply-cont)in the definition of apply-cont.
Whydoestheprogramstillwork?
(cid:3)
Exercise6.2 [ ]Provebyinductiononnthatforanyg,(fib/k n g)=(g(fib n)).

6.1 WritingProgramsinContinuation-PassingStyle 201
(cid:3)
Exercise6.3 [ ] Rewrite each of the following Scheme expressionsin continuation-
passingstyle.AssumethatanyunknownfunctionshavealsobeenrewritteninCPS.
1. (lambda (x y) (p (+ 8 x) (q y)))
2. (lambda (x y u v) (+ 1 (f (g x y) (+ u v))))
3. (+ 1 (f (g x y) (+ u (h v))))
4. (zero? (if a (p x) (p y)))
5. (zero? (if (f a) (p x) (p y)))
6. (let ((x (let ((y 8)) (p y)))) x)
7. (let ((x (if a (p x) (p y)))) x)
(cid:3)(cid:3)
Exercise6.4 [ ]Rewriteeachofthe followingproceduresincontinuation-passing
style.Foreachprocedure,dothisfirstusingadata-structurerepresentationofcontin-
uations,thenwithaproceduralrepresentation,andthenwiththeinlinedprocedural
representation. Last,writetheregisterizedversion. Foreachofthesefourversions,
testtoseethatyourimplementationistail-recursivebydefiningend-contby
(apply-cont (end-cont) val)
= (begin
(eopl:printf "End of computation.~%")
(eopl:printf "This sentence should appear only once.~%")
val)
aswedidinchapter5.
1. remove-first(section1.2.3).
2. list-sum(section1.3).
3. occurs-free?(section1.2.4).
4. subst(section1.2.5).
(cid:3)
Exercise6.5 [ ] When we rewrite an expression in CPS, we choose an evaluation
orderfortheprocedurecallsintheexpression. Rewriteeachoftheprecedingexam-
plesinCPSsothatalltheprocedurecallsareevaluatedfromrighttoleft.
(cid:3)
Exercise6.6 [ ]How many different evaluation orders are possible for the proce-
dure calls in (lambda (x y) (+ (f (g x)) (h (j y))))? For each evalua-
tionorder,writeaCPSexpressionthatcallstheproceduresinthatorder.
(cid:3)(cid:3)
Exercise6.7 [ ] Write out the procedural and the inlined representations for the
interpreterinfigures5.4,5.5,and5.6.

202 6 Continuation-PassingStyle
(cid:3)(cid:3)(cid:3)
Exercise6.8 [ ] Rewrite the interpreter of section 5.4 using a procedural and
inlinedrepresentation.Thisischallengingbecauseweeffectivelyhavetwoobservers,
apply-cont and apply-handler. As a hint, consider modifying the recipe on
page6.1sothatweaddtoeachproceduretwoextraarguments,onerepresentingthe
behaviorofthecontinuationunderapply-contandonerepresentingitsbehavior
underapply-handler.
Sometimes we can find clever representations of continuations. Let’s
fact
reconsider the version of with the procedural representation of con-
tinuations. Therewehadtwocontinuationbuilders,whichwewroteas
(define end-cont
(lambda ()
| (lambda (val) | val))) |     |     |
| ------------- | ------ | --- | --- |
(define fact1-cont
| (lambda (n cont) |          |            |     |
| ---------------- | -------- | ---------- | --- |
| (lambda (val)    | (cont (* | n val))))) |     |
(define apply-cont
| (lambda (cont | val) |     |     |
| ------------- | ---- | --- | --- |
(cont val)))
In this system, all a continuation does is multiply its argument by some
number.(end-cont)multipliesitsargumentby1,andifcontmultipliesits
∗
| valuebyk,then(fact1                   | n cont)multipliesitsvaluebyk |       | n.              |
| ------------------------------------- | ---------------------------- | ----- | --------------- |
| Soeverycontinuationisoftheform(lambda |                              | (val) | (* k val)).This |
means we could representsuch a continuation simply by its lone free vari-
| able,thenumberk. Inthisrepresentationwewouldhave |     |     |     |
| ------------------------------------------------ | --- | --- | --- |
(define end-cont
(lambda ()
1))
(define fact1-cont
| (lambda (n cont) |     |     |     |
| ---------------- | --- | --- | --- |
| (* cont n)))     |     |     |     |
(define apply-cont
| (lambda (cont  | val) |     |     |
| -------------- | ---- | --- | --- |
| (* cont val))) |      |     |     |

6.2 TailForm 203
If we inline these definitions into our original definition of fact/k, and
usethepropertythat(* cont 1)= cont,weget
(define fact
(lambda (n)
(fact/k n 1)))
(define fact/k
(lambda (n cont)
(if (zero? n)
cont
(fact/k (- n 1) (* cont n)))))
Butthisisjustthesameasfact-iter(page139)! Soweseethatanaccu-
mulator is often just a representationof a continuation. This is impressive.
Quite a few classic program optimizations turn out to be instances of this
idea.
(cid:3)
Exercise6.9 [ ] What property of multiplication makes this program optimization
possible?
(cid:3)
Exercise6.10 [ ]Forlist-sum,formulateasuccinctrepresentationofthecontinua-
tions,liketheoneforfact/kabove.
6.2 TailForm
In order to write down a program for converting to continuation-passing
style,weneedtoidentifytheinputandoutputlanguages. Forourinputlan-
guage, we choose the language LETREC, augmented by having multiargu-
mentproceduresandmultideclarationletrecexpressions. Itsgrammaris
showninfigure6.3.WecallthislanguageCPS-IN.Todistinguishtheexpres-
sionsofthislanguagefromthoseofouroutputlanguage,wecalltheseinput
expressions.
TodefinetheclassofpossibleoutputsfromourCPSconversionalgorithm,
weneedtoidentifyasubsetofCPS-INinwhichprocedurecallsneverbuild
anycontrolcontext.
Recallourprinciplefromchapter5:
Itisevaluationofoperands,notthecallingofprocedures,that
makesthecontrolcontextgrow.

| 204 |     |     |     |     | 6 Continuation-PassingStyle |     |
| --- | --- | --- | --- | --- | --------------------------- | --- |
Program::=InpExp
|     |     | a-program | (exp1) |     |     |     |
| --- | --- | --------- | ------ | --- | --- | --- |
InpExp ::=Number
|        |             | const-exp | (num)     |       |     |     |
| ------ | ----------- | --------- | --------- | ----- | --- | --- |
| InpExp | ::=-(InpExp |           | , InpExp) |       |     |     |
|        |             | diff-exp  | (exp1     | exp2) |     |     |
InpExp ::=zero?(InpExp)
|        |       | zero?-exp    | (exp1) |             |        |     |
| ------ | ----- | ------------ | ------ | ----------- | ------ | --- |
| InpExp | ::=if | InpExp       | then   | InpExp else | InpExp |     |
|        |       | if-exp (exp1 |        | exp2 exp3)  |        |     |
InpExp ::=Identifier
|        |            | var-exp            | (var)     |                    |            |              |
| ------ | ---------- | ------------------ | --------- | ------------------ | ---------- | ------------ |
| InpExp | ::=let     | Identifier         | =         | InpExp in          | InpExp     |              |
|        |            | let-exp            | (var      | exp1 body)         |            |              |
| InpExp | ::=letrec  | {Identifier        |           | ({Identifier}∗(,)) | = InpExp}∗ | in InpExp    |
|        |            | letrec-exp         | (p-names  | b-varss            | p-bodies   | letrec-body) |
| InpExp | ::=proc    | ({Identifier}∗(,)) |           | InpExp             |            |              |
|        |            | proc-exp           | (vars     | body)              |            |              |
| InpExp | ::=(InpExp | {InpExp}∗)         |           |                    |            |              |
|        |            | call-exp           | (rator    | rands)             |            |              |
|        |            |                    | Figure6.3 | GrammarforCPS-IN   |            |              |
Thusin
| (define |         | fact      |      |         |              |     |
| ------- | ------- | --------- | ---- | ------- | ------------ | --- |
|         | (lambda | (n)       |      |         |              |     |
|         | (if     | (zero? n) | 1 (* | n (fact | (- n 1)))))) |     |
itisthepositionofthecalltofactasanoperandthatrequiresthecreationof
| acontrolcontext. |     | Bycontrast,in |     |     |     |     |
| ---------------- | --- | ------------- | --- | --- | --- | --- |

6.2 TailForm 205
| (define        | fact-iter     |                |         |             |
| -------------- | ------------- | -------------- | ------- | ----------- |
| (lambda        | (n)           |                |         |             |
| (fact-iter-acc | n             | 1)))           |         |             |
| (define        | fact-iter-acc |                |         |             |
| (lambda        | (n a)         |                |         |             |
| (if            | (zero? n) a   | (fact-iter-acc | (- n 1) | (* n a))))) |
noneofthe procedurecallsisinoperandposition. Wesaythesecallsarein
tail position because their value is the result of the whole call. We refer to
themastailcalls.
WecanalsorecalltheTailCallsDon’tGrowControlContextprinciple:
TailCallsDon’tGrowControlContext
If the value of exp is returned as the value of exp , then exp and exp
|     | 1   |     | 2   | 1 2 |
| --- | --- | --- | --- | --- |
shouldruninthesamecontinuation.
Wesaythatanexpressionisintailformifeveryprocedurecall,andevery
expression containing a procedure call, is in tail position. This condition
impliesthatnoprocedurecallbuildscontrolcontext.
HenceinScheme
| (if (zero? | x) (f y) | (g z)) |     |     |
| ---------- | -------- | ------ | --- | --- |
isintailform,asis
(if b
| (if (zero? | x) (f y) | (g z)) |     |     |
| ---------- | -------- | ------ | --- | --- |
(h u))
but
(+
| (if (zero? | x) (f y) | (g z)) |     |     |
| ---------- | -------- | ------ | --- | --- |
37)
isnotintailform, sincetheifexpression,whichcontainsaprocedurecall,
isnotintailposition.
In general, we must understand the meaning of a language in order to
determineitstailpositions. Asubexpressionintailpositionhastheproperty
thatifitisevaluated,itsvalueimmediatelybecomesthe valueofthe entire

206 6 Continuation-PassingStyle
zero?(O)
-(O , O)
ifOthenTelseT
letVar=OinT
letrec{ Var({ Var }∗( , ))=T }∗inT
proc({ Var }∗( , ))T
(O O ... O)
Figure6.4 TailandoperandpositionsinCPS-IN.TailpositionsaremarkedwithT.
OperandpositionsaremarkedwithO.
expression. Anexpressionmayhavemorethanonetailposition. Forexam-
ple, an if expressionmay choose either the true or the false branch. For a
subexpressionin tailposition, no information need be saved, and therefore
nocontrolcontextneedbebuilt.
The tail positions for CPS-IN are shown in figure 6.4. The value of each
subexpression in tail position could become the value of the entire expres-
sion. Inthecontinuation-passinginterpreter,thesubexpressionsinoperand
positions are the ones that require building new continuations. The subex-
pressionsintailpositionareevaluatedinthesamecontinuationastheorigi-
nalexpression,asillustratedonpage152.
WeusethisdistinctiontodesignatargetlanguageCPS-OUTforourCPS
conversionalgorithm. Thegrammarforthislanguageisshowninfigure6.5.
ThisgrammardefinesasubsetofCPS-IN,butwithadifferentgrammar. Its
productionnamesalwaysbeginwithcps-,sotheywillnotbeconfusedwith
theproductionnamesinCPS-IN.
The new grammar has two nonterminals, SimpleExp and TfExp. It is
designed so that expressions in SimpleExp are guaranteed never to contain
anyprocedurecalls,andsothatexpressionsinTfExpareguaranteedtobein
tailform.
Expressions in SimpleExp are guaranteed to never contain any procedure
calls, so they correspond roughly to simple straight-line code, and for our
purposes we consider them too simple to require any use of the control
stack.Simpleexpressionsincludeprocexpressions,sinceaprocexpression
returnsimmediatelywithaprocedurevalue,butthebodyofthatprocedure
mustbeintailform.

| 6.2 TailForm |     |     | 207 |
| ------------ | --- | --- | --- |
A continuation-passing interpreter for tail-form expressions is shown in
figure 6.6. Since procedures in this language take multiple arguments, we
use extend-env* from exercise 2.10 to create multiple bindings, and we
similarlyextendextend-env-rectogetextend-env-rec*.
Inthisinterpreter,alltherecursivecallsareintailposition(inScheme),so
runningtheinterpreterbuildsnocontrolcontextinScheme. (Thisisn’tquite
true: the procedure value-of-simple-exp(exercise 6.11) builds control
contextinScheme,butthatcanbefixed(seeexercise6.18).)
More importantly, the interpreter creates no new continuations. The
procedure value-of/k takes one continuation argument and passes it
unchanged in every recursive call. So we could easily have removed the
continuationargumententirely.
Ofcourse,thereisnocompletelygeneralwayofdeterminingwhetherthe
controlbehaviorofaprocedureisiterativeornot. Consider
| (lambda    | (n)                 |     |     |
| ---------- | ------------------- | --- | --- |
| (if        | (strange-predicate? | n)  |     |
| (fact      | n)                  |     |     |
| (fact-iter | n)))                |     |     |
strange-predicate?returns
| This procedure | is iterative | only if | false for |
| -------------- | ------------ | ------- | --------- |
allsufficientlylargevaluesofn.Butitisnotalwayspossibletodeterminethe
truthorfalsityofthiscondition,evenifitispossibletoexaminethecodeof
| strange-predicate?. |     | Thereforethebestwecanhopeforistomakesure |     |
| ------------------- | --- | ---------------------------------------- | --- |
thatnoprocedurecallintheprogramwillbuildupcontrolcontext,whether
ornotitisactuallyexecuted.
(cid:3)
Exercise6.11 [ ] Complete the interpreter of figure 6.6 by writing value-of-
simple-exp.
(cid:3)
Exercise6.12 [ ] Determinewhethereachofthefollowingexpressionsissimple.
1. -((f -(x,1)),1)
2. (f -(-(x,y),1))
| 3. if zero?(x) | then       | -(x,y) else -(-(x,y),1) |     |
| -------------- | ---------- | ----------------------- | --- |
| 4. let x       | = proc (y) | (y x) in -(x,3)         |     |
| 5. let f       | = proc (x) | x in (f 3)              |     |

| 208     |           |     |        |     | 6   | Continuation-PassingStyle |     |
| ------- | --------- | --- | ------ | --- | --- | ------------------------- | --- |
| Program | ::=TfExp  |     |        |     |     |                           |     |
|         | a-program |     | (exp1) |     |     |                           |     |
SimpleExp::=Number
|     | const-exp |     | (num) |     |     |     |     |
| --- | --------- | --- | ----- | --- | --- | --- | --- |
SimpleExp::=Identifier
|                         | var-exp      | (var) |              |          |     |     |     |
| ----------------------- | ------------ | ----- | ------------ | -------- | --- | --- | --- |
| SimpleExp::=-(SimpleExp |              |       | , SimpleExp) |          |     |     |     |
|                         | cps-diff-exp |       | (simple1     | simple2) |     |     |     |
SimpleExp::=zero?(SimpleExp)
|                  | cps-zero?-exp   |                 | (simple1)          |        |         |          |          |
| ---------------- | --------------- | --------------- | ------------------ | ------ | ------- | -------- | -------- |
| SimpleExp::=proc |                 | ({Identifier}∗) | TfExp              |        |         |          |          |
|                  | cps-proc-exp    |                 | (vars              | body)  |         |          |          |
| TfExp            | ::=SimpleExp    |                 |                    |        |         |          |          |
|                  | simple-exp->exp |                 | (simple-exp1)      |        |         |          |          |
| TfExp            | ::=let          | Identifier      | = SimpleExp        | in     | TfExp   |          |          |
|                  | cps-let-exp     |                 | (var simple1       |        | body)   |          |          |
| TfExp            | ::=letrec       | {Identifier     | ({Identifier}∗(,)) |        | =       | TfExp}∗  | in TfExp |
|                  | cps-letrec-exp  |                 | (p-names           |        | b-varss | p-bodies | body)    |
| TfExp            | ::=if SimpleExp |                 | then TfExp         | else   | TfExp   |          |          |
|                  | cps-if-exp      |                 | (simple1           | body1  | body2)  |          |          |
| TfExp            | ::=(SimpleExp   |                 | {SimpleExp}∗)      |        |         |          |          |
|                  | cps-call-exp    |                 | (rator             | rands) |         |          |          |
|                  |                 | Figure6.5       | GrammarforCPS-OUT  |        |         |          |          |

6.2 TailForm 209
value-of/k : TfExp × Env × Cont → FinalAnswer
(define value-of/k
(lambda (exp env cont)
(cases tfexp exp
(simple-exp->exp (simple)
(apply-cont cont
(value-of-simple-exp simple env)))
(let-exp (var rhs body)
(let ((val (value-of-simple-exp rhs env)))
(value-of/k body
(extend-env (list var) (list val) env)
cont)))
(letrec-exp (p-names b-varss p-bodies letrec-body)
(value-of/k letrec-body
(extend-env-rec** p-names b-varss p-bodies env)
cont))
(if-exp (simple1 body1 body2)
(if (expval->bool (value-of-simple-exp simple1 env))
(value-of/k body1 env cont)
(value-of/k body2 env cont)))
(call-exp (rator rands)
(let ((rator-proc
(expval->proc
(value-of-simple-exp rator env)))
(rand-vals
(map
(lambda (simple)
(value-of-simple-exp simple env))
rands)))
(apply-procedure/k rator-proc rand-vals cont))))))
apply-procedure : Proc × ExpVal → ExpVal
(define apply-procedure/k
(lambda (proc1 args cont)
(cases proc proc1
(procedure (vars body saved-env)
(value-of/k body
(extend-env* vars args saved-env)
cont)))))
Figure6.6 Interpreterfortail-formexpressionsinCPS-OUT.

210 6 Continuation-PassingStyle
(cid:3)
Exercise6.13 [ ] Translate each of these expressions in CPS-IN into continuation-
passingstyle using the CPS recipeon page 200 above. Testyour transformedpro-
gramsbyrunningthemusingtheinterpreteroffigure6.6. Besurethattheoriginal
andtransformedversionsgivethesameansweroneachinput.
1. removeall.
letrec
removeall(n,s) =
if null?(s)
then emptylist
else if number?(car(s))
then if equal?(n,car(s))
then (removeall n cdr(s))
else cons(car(s),
(removeall n cdr(s)))
else cons((removeall n car(s)),
(removeall n cdr(s)))
2. occurs-in?.
letrec
occurs-in?(n,s) =
if null?(s)
then 0
else if number?(car(s))
then if equal?(n,car(s))
then 1
else (occurs-in? n cdr(s))
else if (occurs-in? n car(s))
then 1
else (occurs-in? n cdr(s))
3. remfirst.Thisusesoccurs-in?fromtheprecedingexample.
letrec
remfirst(n,s) =
letrec
loop(s) =
if null?(s)
then emptylist
else if number?(car(s))
then if equal?(n,car(s))
then cdr(s)
else cons(car(s),(loop cdr(s)))
else if (occurs-in? n car(s))
then cons((remfirst n car(s)),
cdr(s))
else cons(car(s),
(remfirst n cdr(s)))
in (loop s)

6.2 TailForm 211
4. depth.
letrec
depth(s) =
if null?(s)
then 1
else if number?(car(s))
| then (depth cdr(s))       |           |     |
| ------------------------- | --------- | --- |
| else if less?(add1((depth | car(s))), |     |
(depth cdr(s)))
| then (depth      | cdr(s))  |     |
| ---------------- | -------- | --- |
| else add1((depth | car(s))) |     |
5. depth-with-let.
letrec
depth(s) =
if null?(s)
then 1
else if number?(car(s))
| then (depth cdr(s))       |                  |          |
| ------------------------- | ---------------- | -------- |
| else let dfirst           | = add1((depth    | car(s))) |
| drest                     | = (depth cdr(s)) |          |
| in if less?(dfirst,drest) |                  |          |
then drest
else dfirst
6. map.
letrec
| map(f, l) = if null?(l) |          |     |
| ----------------------- | -------- | --- |
| then emptylist          |          |     |
| else cons((f            | car(l)), |     |
(map f cdr(l)))
square(n) = *(n,n)
in (map square list(1,2,3,4,5))
7. fnlrgtn.Thisproceduretakesann-list,likeans-list(page9),butwithnumbers
insteadofsymbols,andanumbernandreturnsthefirstnumberinthelist(inleft-
to-rightorder)thatisgreaterthann.Oncetheresultisfound,nofurtherelements
inthelistareexamined.Forexample,
(fnlrgtn list(1,list(3,list(2),7,list(9)))
6)
finds7.

212 6 Continuation-PassingStyle
8. every.Thisproceduretakesapredicateandalistandreturnsatruevalueifand
onlyifthepredicateholdsforeachlistelement.
letrec
|     | every(pred, | l) = |     |
| --- | ----------- | ---- | --- |
if null?(l)
then 1
|     | else if   | (pred car(l))        |                |
| --- | --------- | -------------------- | -------------- |
|     | then      | (every pred cdr(l))  |                |
|     | else      | 0                    |                |
|     | in (every | proc(n)greater?(n,5) | list(6,7,8,9)) |
(cid:3)
Exercise6.14 [ ] Completetheinterpreteroffigure6.6bysupplyingdefinitionsfor
value-of-programandapply-cont.
Exercise6.15 [ (cid:3) ] Observe that in the interpreter of the preceding exercise, there is
onlyonepossiblevalueforcont.Usethisobservationtoremovethecontargument
entirely.
| Exercise6.16 | [ (cid:3) ]Registerizetheinterpreteroffigure6.6. |     |     |
| ------------ | ------------------------------------------------ | --- | --- |
(cid:3)
| Exercise6.17 | [ ]Trampolinetheinterpreteroffigure6.6. |     |     |
| ------------ | --------------------------------------- | --- | --- |
(cid:3)(cid:3) ]ModifythegrammarofCPS-OUTsothatasimplediff-expor
| Exercise6.18 | [   |     |     |
| ------------ | --- | --- | --- |
zero?-expcanhaveonlyaconstantorvariableasanargument. Thusintheresult-
inglanguagevalue-of-simple-expcanbemadenonrecursive.
Exercise6.19 [ (cid:3)(cid:3) ]WriteaSchemeproceduretail-form?thattakesthesyntaxtree
of a program in CPS-IN, expressed in the grammar of figure 6.3, and determines
whetherthesamestringwouldbeintailformaccordingtothegrammaroffigure6.5.
| 6.3 Convertingto | Continuation-Passing |     | Style |
| ---------------- | -------------------- | --- | ----- |
In this section we develop an algorithm for transforming any program in
CPS-INtoCPS-OUT.
Like the continuation-passing interpreter, our translator will Follow the
Grammar. Also like the continuation-passing interpreter,our translatorwill
takeanadditionalargumentthatrepresentsacontinuation. Thisadditional
argumentwillbeasimpleexpressionthatrepresentsthecontinuation.
Aswehavedoneinthepast,wewillproceedfromexamplestoaspecifi-
cation,andfromaspecificationtoaprogram. Figure6.7showsasomewhat
moredetailedversionofthemotivatingexamples,writteninSchemesothat
theywillbesimilartothoseoftheprecedingsection.

6.3 ConvertingtoContinuation-PassingStyle 213
| (lambda (x) |     |     |     |     |
| ----------- | --- | --- | --- | --- |
(cond
| ((zero? | x) 17)   |         |               |     |
| ------- | -------- | ------- | ------------- | --- |
| ((= x   | 1) (f (- | x 13)   | 7))           |     |
| ((= x   | 2) (+ 22 | (- x 3) | x))           |     |
| ((= x   | 3) (+ 22 | (f x)   | 37))          |     |
| ((= x   | 4) (g 22 | (f x))) |               |     |
| ((= x   | 5) (+ 22 | (f x)   | 33 (g y)))    |     |
| (else   | (h (f x) | (- 44   | y) (g y)))))) |     |
becomes
| (lambda (x | k)  |     |     |     |
| ---------- | --- | --- | --- | --- |
(cond
| ((zero?   | x) (k 17))                                   |           |              |                  |
| --------- | -------------------------------------------- | --------- | ------------ | ---------------- |
| ((= x     | 1) (f (-                                     | x 13)     | 7 k))        |                  |
| ((= x     | 2) (k (+                                     | 22 (-     | x 3) x)))    |                  |
| ((= x     | 3) (f x (lambda                              |           | (v1) (k      | (+ 22 v1 37))))) |
| ((= x     | 4) (f x (lambda                              |           | (v1) (g      | 22 v1 k))))      |
| ((= x     | 5) (f x (lambda                              |           | (v1)         |                  |
|           |                                              | (g y      | (lambda (v2) |                  |
|           |                                              |           | (k (+ 22     | v1 33 v2))))))   |
| (else     | (f x (lambda                                 | (v1)      |              |                  |
|           | (g                                           | y (lambda | (v2)         |                  |
|           |                                              | (h        | v1 (- 44     | y) v2 k))))))))  |
| Figure6.7 | MotivatingexamplesforCPSconversion(inScheme) |           |              |                  |
Thefirstcaseisthatofaconstant. Constantsarejustsenttothecontinua-
| tion,asinthe(zero? | x)lineabove. |       |     |     |
| ------------------ | ------------ | ----- | --- | --- |
| (cps-of-exp        | n K) =       | (K n) |     |     |
HereKissomesimple-expthatdenotesacontinuation.
Similarly,variablesarejustsenttothecontinuation.
| (cps-of-exp | var K) | = (K | var) |     |
| ----------- | ------ | ---- | ---- | --- |

214 6 Continuation-PassingStyle
Of course, the input and output of our algorithm will be abstract syntax
trees,soweshouldhavewrittenthebuildersfortheabstractsyntaxinstead
oftheconcretesyntax,like
| (cps-of-exp |                    |     | (const-exp |      | n) K)          |     |       |     |     |
| ----------- | ------------------ | --- | ---------- | ---- | -------------- | --- | ----- | --- | --- |
| =           | (make-send-to-cont |     |            | K    | (cps-const-exp |     | n))   |     |     |
| (cps-of-exp |                    |     | (var-exp   | var) | K)             |     |       |     |     |
| =           | (make-send-to-cont |     |            | K    | (cps-var-exp   |     | var)) |     |     |
where
|                   |               |                   |     |             | ×              | →   |       |     |     |
| ----------------- | ------------- | ----------------- | --- | ----------- | -------------- | --- | ----- | --- | --- |
| make-send-to-cont |               |                   | :   | SimpleExp   | SimpleExp      |     | TfExp |     |     |
| (define           |               | make-send-to-cont |     |             |                |     |       |     |     |
|                   | (lambda       | (k-exp            |     | simple-exp) |                |     |       |     |     |
|                   | (cps-call-exp |                   |     | k-exp (list | simple-exp)))) |     |       |     |     |
list
We need the since in CPS-OUT every call expression takes a list of
operands.
We will, however, continue to use concrete syntax in our specifications
becausetheconcretesyntaxisgenerallyeasiertoread.
| Whataboutprocedures? |     |     |     | Weconvertaprocedure,likethe(lambda |     |     |     |     | (x) |
| -------------------- | --- | --- | --- | ---------------------------------- | --- | --- | --- | --- | --- |
...) infigure6.7,byaddinganadditionalparameterkandconvertingthe
body to send its value to the continuation k. This is just what we did in
| figure6.7. |      | So     |     |           |     |     |     |     |     |
| ---------- | ---- | ------ | --- | --------- | --- | --- | --- | --- | --- |
| proc       | (var | , ..., |     | var ) exp |     |     |     |     |     |
|            |      | 1      |     | n         |     |     |     |     |     |
becomes
| proc | (var | , ..., |     | var , k) | (cps-of-exp |     | exp k) |     |     |
| ---- | ---- | ------ | --- | -------- | ----------- | --- | ------ | --- | --- |
|      |      | 1      |     | n        |             |     |        |     |     |
asinthefigure. However,thisdoesn’tquitefinish thejob. Ourgoalwasto
producecode thatwould evaluatethe procexpressionandsend the result
tothecontinuation K. Sotheentirespecificationforaprocexpressionis
| (cps-of-exp |           |     | <<proc | (var   | , ...,   | var )       | exp>> | K)        |     |
| ----------- | --------- | --- | ------ | ------ | -------- | ----------- | ----- | --------- | --- |
|             |           |     |        | 1      |          | n           |       |           |     |
| =           | (K <<proc |     | (var   | , ..., | var , k) | (cps-of-exp |       | exp k)>>) |     |
|             |           |     | 1      |        | n        |             |       |           |     |
Here k is a fresh variable, and K is an arbitrary simple expression that
denotesacontinuation.

| 6.3 ConvertingtoContinuation-PassingStyle |     |     |     |     |     |                        |     | 215 |
| ----------------------------------------- | --- | --- | --- | --- | --- | ---------------------- | --- | --- |
| Whataboutexpressionsthathaveoperands?     |     |     |     |     |     | Letusadd,forthemoment, |     |     |
a sum expression to our language, with arbitrarily many operands. To do
this,weaddtothegrammarofCPS-INtheproduction
Expression::=+({InpExp}∗(,))
|     |     |     |     | sum-exp | (exps) |     |     |     |
| --- | --- | --- | --- | ------- | ------ | --- | --- | --- |
andtothegrammarofCPS-OUTtheproduction
SimpleExp::=+({SimpleExp}∗(,))
|     |     |     | cps-sum-exp |     | (simple-exps) |     |     |     |
| --- | --- | --- | ----------- | --- | ------------- | --- | --- | --- |
This new production preserves the property that no procedure call ever
appearsinsideasimpleexpression.
What are the possibilities for (cps-of-exp «+(exp , ..., exp )»
|     |     |     |     |     |     |     | 1   | n   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
K)? It could be that all of exp 1 , ..., exp n are simple, as in the (= x 2)
caseinfigure6.7. Thentheentiresumexpressionissimple,andwecanjust
passitto the continuation. We let simp denote asimple expression. In this
casewecansay
| (cps-of-exp                        |        | <<+(simp | ,      | ..., simp | )>>                      | K)  |     |     |
| ---------------------------------- | ------ | -------- | ------ | --------- | ------------------------ | --- | --- | --- |
|                                    |        |          | 1      |           | n                        |     |     |     |
| = (K                               | +(simp | , ...,   |        | ))        |                          |     |     |     |
|                                    | 1      |          | simp n |           |                          |     |     |     |
| Whatifoneoftheoperandsisnonsimple? |        |          |        |           | Thenweneedtoevaluateitin |     |     |     |
acontinuationthatgivesanametoitsvalueandproceedswiththesum, as
inthe(= x 3)caseabove. Therethesecondoperandisthefirstnonsimple
one. ThenourCPSconvertershouldhavethepropertythat
| (cps-of-exp   |     | <<+(simp | ,   | ,          | ,   | ..., | )>> | K)  |
| ------------- | --- | -------- | --- | ---------- | --- | ---- | --- | --- |
|               |     |          | 1   | exp 2 simp | 3   | simp | n   |     |
| = (cps-of-exp |     | exp      |     |            |     |      |     |     |
2
| <<proc |     | (var 2 ) | (K +(simp | 1 , | var 2 , simp | 3 , | ..., simp | n ))>> |
| ------ | --- | -------- | --------- | --- | ------------ | --- | --------- | ------ |
If exp is just a procedure call, then the output will look like the one
2
in the figure. But exp might be more complicated, so we recur, calling
2
cps-of-exponexp
|      |           | 2 andthelargercontinuation |     |            |        |      |     |     |
| ---- | --------- | -------------------------- | --- | ---------- | ------ | ---- | --- | --- |
| proc | (var ) (K | +(simp                     | ,   | var , simp | , ..., | simp | ))  |     |
|      | 2         |                            | 1   | 2          | 3      |      | n   |     |

216 6 Continuation-PassingStyle
There might, however, be other nonsimple operands in the sum expres-
sion,asthereareinthe(= x 5)case.Soinsteadofsimplyusingthecontin-
uation
proc (var ) (K +(simp , var , simp , ..., simp ))
2 1 2 3 n
weneedtorecuronthelaterargumentsaswell. Wecansummarizethisrule
as
(cps-of-exp <<+(simp , exp , exp , ..., exp )>> K)
1 2 3 n
= (cps-of-exp exp
2
<<proc (var )
2
(cps-of-exp <<+(simp , var , exp , ..., exp )>> K))
1 2 3 n
Eachoftherecursivecallstocps-of-expisguaranteedtoterminate.The
firstcallterminatesbecauseexp issmallerthantheoriginalexpression. The
2
secondcallterminatesbecauseitsargumentisalsosmallerthantheoriginal:
var isalwayssmallerthanexp .
2 2
Forexample,lookingatthe(= x 5)lineandusingthesyntaxofCPS-IN,
wehave
(cps-of-exp <<+((f x), 33, (g y))>> K)
= (cps-of-exp <<(f x)>>
<<proc (v1)
(cps-of-exp +(v1, 33, (g y)) K)>>)
= (cps-of-exp <<(f x)>>
<<proc (v1)
(cps-of-exp <<(g y)>>
<<proc (v2)
(cps-of-exp <<+(v1, 33, v2)>> K)))
= (cps-of-exp <<(f x)>>
<<proc (v1)
(cps-of-exp <<(g y)>>
<<proc (v2)
(K <<+(v1, 33, v2)>>)))
= (f x
proc (v1)
(g y
proc (v2)
(K +(v1, 33, v2))))

6.3 ConvertingtoContinuation-PassingStyle 217
Procedure calls work the same way. If both the operator and all the
operands are simple, then we just call the procedure with a continuation
argument,asinthe(= x 2)line.
(cps-of-exp <<(simp simp ... simp )>> K)
0 1 n
= (simp simp ... simp K)
0 1 n
If, on the other hand, one of the operands is nonsimple, then we must
causeittobeevaluatedfirst,asinthe(= x 4)line.
(cps-of-exp <<(simp simp exp exp ... exp )>> K)
0 1 2 3 n
= (cps-of-exp exp
2
<<proc (var )
2
(cps-of-exp <<(simp simp var exp ... exp )>> K)>>)
0 1 2 3 n
And, asbefore, the second callto cps-of-expwill recur down the pro-
cedurecall,callingcps-of-expforeachofthenonsimplearguments,until
thereareonlysimpleargumentsleft.
Hereishowtheseruleshandlethe(= x 5)example,writteninCPS-IN.
(cps-of-exp <<(h (f x) -(44,y) (g y))>> K)
= (cps-of-exp <<(f x)>>
<<proc (v1)
(cps-of-exp <<(h v1 -(44,y) (g y))>> K)>>)
= (f x
proc (v1)
(cps-of-exp <<(h v1 -(44,y) (g y))>> K)>>)
= (f x
proc (v1)
(cps-of-exp <<(g y)>>
<<proc (v2)
(cps-of-exp <<(h v1 -(44,y) v2)>> K)))
= (f x
proc (v1)
(g y
proc (v2)
(cps-of-exp <<(h v1 -(44,y) v2)>> K)))
= (f x
proc (v1)
(g y
proc (v2)
(h v1 -(44,y) v2 K)))
Thespecificationsforsumexpressionsandprocedurecallsfollowasimilar
pattern: they find the first nonsimple operand and recur on that operand

218 6 Continuation-PassingStyle
and on the modified list of operands. This works for any expression that
evaluates its operands. If complex-exp is some CPS-IN expression that
evaluatesitsoperands,thenweshouldhave
(cps-of-exp (complex-exp simp simp exp exp ... exp ) K)
0 1 2 3 n
= (cps-of-exp exp
2
<<proc (var )
2
(cps-of-exp
(complex-exp simp simp var exp ... exp )
0 1 2 3 n
K)>>)
wherevar isafreshvariable.
2
The only time that the treatment of sum expressions and procedurecalls
differsiswhentheargumentsareallsimple. Inthatcase,weneedtoconvert
eachoftheargumentstoaCPS-OUTsimple-expandproduceatail-form
expressionwiththeresults.
We can encapsulate this behavior into the procedure cps-of-exps,
showninfigure6.8. Itsargumentsarealistofinputexpressionsandapro-
cedure builder. It finds the position of the first nonsimple expression in
thelist,usingtheprocedurelist-indexfromexercise1.23. Ifthereissuch
anonsimpleexpression,thenitisconvertedinacontinuationthatgivesthe
valueaname(theidentifierboundtovar)andrecursdownthemodifiedlist
ofexpressions.
If there are no nonsimple expressions, then we would like to apply
builder to the list of expressions. However, although these expressions
are simple, they are still in the grammar of CPS-IN. Therefore we con-
vert each expression to the grammar of CPS-OUT using the procedure
cps-of-simple-exp. We then send the list of SimpleExps to builder.
(list-setisdescribedinexercise1.19.)
The procedure inp-exp-simple? takes an expression in CPS-IN and
determineswhetheritsstringwouldbeparseableasaSimpleExp. Itusesthe
procedure every? from exercise 1.24. The expression (every? pred lst)
returns#tifeveryelementoflstsatisfiespred,andreturns#fotherwise.
The code forcps-of-simple-exp is straightforward and is shown in
figure6.9. Italsotranslatesthebodyofaproc-expintoCPS,whichisnec-
essaryfortheoutputtobeaSimpleExp.
Wecangeneratetail-formexpressionsforsumexpressionsandprocedure
callsusingcps-of-exps.

| 6.3 ConvertingtoContinuation-PassingStyle |             |                  |                        |         |                  |        |        | 219   |
| ----------------------------------------- | ----------- | ---------------- | ---------------------- | ------- | ---------------- | ------ | ------ | ----- |
|                                           |             |                  |                        | ×       |                  | →      | →      |       |
| cps-of-exps                               |             | : Listof(InpExp) |                        |         | (Listof(InpExp)  | TfExp) |        | TfExp |
| (define                                   | cps-of-exps |                  |                        |         |                  |        |        |       |
| (lambda                                   |             | (exps            | builder)               |         |                  |        |        |       |
|                                           | (let        | cps-of-rest      |                        | ((exps  | exps))           |        |        |       |
|                                           | cps-of-rest |                  | : Listof(InpExp)→TfExp |         |                  |        |        |       |
|                                           | (let        | ((pos            | (list-index            |         |                  |        |        |       |
|                                           |             |                  |                        | (lambda | (exp)            |        |        |       |
|                                           |             |                  |                        | (not    | (inp-exp-simple? |        | exp))) |       |
exps)))
|     | (if | (not     | pos)  |                        |     |         |     |     |
| --- | --- | -------- | ----- | ---------------------- | --- | ------- | --- | --- |
|     |     | (builder |       | (map cps-of-simple-exp |     | exps))  |     |     |
|     |     | (let     | ((var | (fresh-identifier      |     | ’var))) |     |     |
(cps-of-exp
|     |     |     | (list-ref     | exps | pos)       |     |     |     |
| --- | --- | --- | ------------- | ---- | ---------- | --- | --- | --- |
|     |     |     | (cps-proc-exp |      | (list var) |     |     |     |
(cps-of-rest
|                 |                 |                  | (list-set      |                  | exps pos        | (var-exp         | var))))))))))) |         |
| --------------- | --------------- | ---------------- | -------------- | ---------------- | --------------- | ---------------- | -------------- | ------- |
|                 |                 |                  | :              | →                |                 |                  |                |         |
| inp-exp-simple? |                 |                  | InpExp         |                  | Bool            |                  |                |         |
| (define         | inp-exp-simple? |                  |                |                  |                 |                  |                |         |
| (lambda         |                 | (exp)            |                |                  |                 |                  |                |         |
|                 | (cases          | expression       |                | exp              |                 |                  |                |         |
|                 | (const-exp      |                  | (num)          | #t)              |                 |                  |                |         |
|                 | (var-exp        |                  | (var)          | #t)              |                 |                  |                |         |
|                 | (diff-exp       |                  | (exp1          | exp2)            |                 |                  |                |         |
|                 | (and            | (inp-exp-simple? |                |                  | exp1)           | (inp-exp-simple? |                | exp2))) |
|                 | (zero?-exp      |                  | (exp1)         | (inp-exp-simple? |                 | exp1))           |                |         |
|                 | (proc-exp       |                  | (ids           | exp)             | #t)             |                  |                |         |
|                 | (sum-exp        |                  | (exps)         | (every?          | inp-exp-simple? |                  | exps))         |         |
|                 | (else           | #f))))           |                |                  |                 |                  |                |         |
|                 |                 |                  |                | Figure6.8        | cps-of-exps     |                  |                |         |
|                 |                 |                  |                |                  | ×               | →                |                |         |
| cps-of-diff-exp |                 | :                | Listof(InpExp) |                  | SimpleExp       | TfExp            |                |         |
| (define         | cps-of-sum-exp  |                  |                |                  |                 |                  |                |         |
| (lambda         |                 | (exps            | k-exp)         |                  |                 |                  |                |         |
|                 | (cps-of-exps    |                  | exps           |                  |                 |                  |                |         |
|                 | (lambda         |                  | (simples)      |                  |                 |                  |                |         |
(make-send-to-cont
k-exp
|     |     | (cps-sum-exp |     | simples)))))) |     |     |     |     |
| --- | --- | ------------ | --- | ------------- | --- | --- | --- | --- |

220 6 Continuation-PassingStyle
cps-of-simple-exp : InpExp → SimpleExp
usage: assumes (inp-exp-simple? exp).
(define cps-of-simple-exp
(lambda (exp)
(cases expression exp
(const-exp (num) (cps-const-exp num))
(var-exp (var) (cps-var-exp var))
(diff-exp (exp1 exp2)
(cps-diff-exp
(cps-of-simple-exp exp1)
(cps-of-simple-exp exp2)))
(zero?-exp (exp1)
(cps-zero?-exp (cps-of-simple-exp exp1)))
(proc-exp (ids exp)
(cps-proc-exp (append ids (list ’k%00))
(cps-of-exp exp (cps-var-exp ’k%00))))
(sum-exp (exps)
(cps-sum-exp (map cps-of-simple-exp exps)))
(else
(report-invalid-exp-to-cps-of-simple-exp exp)))))
Figure6.9 cps-of-simple-exp
cps-of-call-exp : InpExp × Listof(InpExp) × SimpleExp → TfExp
(define cps-of-call-exp
(lambda (rator rands k-exp)
(cps-of-exps (cons rator rands)
(lambda (simples)
(cps-call-exp
(car simples)
(append (cdr simples) (list k-exp)))))))
We can now write the rest of our CPS translator (figures 6.10–6.12). It
Follows the Grammar. When the expression is always simple, as for con-
stants, variables, and procedures, we generate the code immediately using
make-send-to-cont. Otherwise, we call an auxiliary procedure. Each
auxiliaryprocedurecallscps-of-expstoevaluateitssubexpressions,sup-
plyinganappropriatebuildertoconstructtheinnermostportionoftheCPS
output. The one exception is cps-of-letrec-exp, which has no imme-
diate subexpressions, so it generates the CPS output directly. Finally, we
translateaprogrambycallingcps-of-expsonthewholeprogram,witha
builderthatjustreturnsthevalueofthesimple.

6.3 ConvertingtoContinuation-PassingStyle 221
Forthefollowingexercises,makesurethatyouroutputexpressionsarein
tailformbyrunningthemusingthegrammarandinterpreterforCPS-OUT.
(cid:3)
Exercise6.20 [ ] Our procedurecps-of-expscauses subexpressionsto be evalu-
atedfromlefttoright. Modifycps-of-expssothatsubexpressionsareevaluated
fromrighttoleft.
(cid:3)
Exercise6.21 [ ] Modify cps-of-call-exp so that the operands are evaluated
fromlefttoright,followedbytheoperator.
|              | (cid:3) Sometimes,whenwegenerate(K |     | simp),Kisalreadyaproc-exp. |     |
| ------------ | ---------------------------------- | --- | -------------------------- | --- |
| Exercise6.22 | [ ]                                |     |                            |     |
Soinsteadofgenerating
|     | (proc (var ) ... | simp) |     |     |
| --- | ---------------- | ----- | --- | --- |
1
wecouldgenerate
|     | let var = simp |     |     |     |
| --- | -------------- | --- | --- | --- |
1
|     | in ... |     |     |     |
| --- | ------ | --- | --- | --- |
ThisleadstoCPScodewiththepropertythatitnevercontainsasubexpressionofthe
form
|     | (proc (var) exp | 1   |     |     |
| --- | --------------- | --- | --- | --- |
simp)
unlessthatsubexpressionwasintheoriginalexpression.
Modifymake-send-to-conttogeneratethisbettercode. Whendoesthenewrule
apply?
(cid:3)(cid:3)
Exercise6.23 [ ]Observethatourruleforifmakestwocopiesofthecontinuation
K, so in a nested if the size of the transformed programcan grow exponentially.
Runanexampletoconfirmthisobservation.Thenshowhowthismaybeavoidedby
changingthetransformationtobindafreshvariabletoK.
(cid:3)(cid:3)
Exercise6.24 [ ] Addliststothelanguage(exercise3.10).Rememberthattheargu-
mentstoalistarenotintailposition.
(cid:3)(cid:3)
Exercise6.25 [ ] ExtendCPS-INsothataletexpressioncandeclareanarbitrary
numberofvariables(exercise3.16).
(cid:3)(cid:3)
Exercise6.26 [ ] A continuation variable introduced by cps-of-exps will only
occuronceinthecontinuation.Modifymake-send-to-contsothatinsteadofgen-
erating
|                              | let var 1 = simp 1                                   |          |                      |            |
| ---------------------------- | ---------------------------------------------------- | -------- | -------------------- | ---------- |
|                              | in T                                                 |          |                      |            |
|                              |                                                      | /var     |                      | /var]means |
| asinexercise6.22,itgenerates |                                                      | T[simp 1 | 1 ],wherethenotation | E 1 [E 2   |
| expressionE                  | 1 witheveryfreeoccurrenceofthevariablevarreplacedbyE |          |                      | 2 .        |

222 6 Continuation-PassingStyle
cps-of-exp : InpExp × SimpleExp → TfExp
(define cps-of-exp
(lambda (exp k-exp)
(cases expression exp
(const-exp (num)
(make-send-to-cont k-exp (cps-const-exp num)))
(var-exp (var)
(make-send-to-cont k-exp (cps-var-exp var)))
(proc-exp (vars body)
(make-send-to-cont k-exp
(cps-proc-exp (append vars (list ’k%00))
(cps-of-exp body (cps-var-exp ’k%00)))))
(zero?-exp (exp1)
(cps-of-zero?-exp exp1 k-exp))
(diff-exp (exp1 exp2)
(cps-of-diff-exp exp1 exp2 k-exp))
(sum-exp (exps)
(cps-of-sum-exp exps k-exp))
(if-exp (exp1 exp2 exp3)
(cps-of-if-exp exp1 exp2 exp3 k-exp))
(let-exp (var exp1 body)
(cps-of-let-exp var exp1 body k-exp))
(letrec-exp (p-names b-varss p-bodies letrec-body)
(cps-of-letrec-exp
p-names b-varss p-bodies letrec-body k-exp))
(call-exp (rator rands)
(cps-of-call-exp rator rands k-exp)))))
cps-of-zero?-exp : InpExp × SimpleExp → TfExp
(define cps-of-zero?-exp
(lambda (exp1 k-exp)
(cps-of-exps (list exp1)
(lambda (simples)
(make-send-to-cont
k-exp
(cps-zero?-exp
(car simples)))))))
Figure6.10 cps-of-exp,part1

6.3 ConvertingtoContinuation-PassingStyle 223
cps-of-diff-exp : InpExp × InpExp × SimpleExp → TfExp
(define cps-of-diff-exp
(lambda (exp1 exp2 k-exp)
(cps-of-exps
(list exp1 exp2)
(lambda (simples)
(make-send-to-cont
k-exp
(cps-diff-exp
(car simples)
(cadr simples)))))))
cps-of-if-exp : InpExp × InpExp × InpExp × SimpleExp → TfExp
(define cps-of-if-exp
(lambda (exp1 exp2 exp3 k-exp)
(cps-of-exps (list exp1)
(lambda (simples)
(cps-if-exp (car simples)
(cps-of-exp exp2 k-exp)
(cps-of-exp exp3 k-exp))))))
cps-of-let-exp : Var × InpExp × InpExp × SimpleExp → TfExp
(define cps-of-let-exp
(lambda (id rhs body k-exp)
(cps-of-exps (list rhs)
(lambda (simples)
(cps-let-exp id
(car simples)
(cps-of-exp body k-exp))))))
cps-of-letrec-exp :
Listof(Var) × Listof(Listof(Var)) × Listof(InpExp) × SimpleExp → TfExp
(define cps-of-letrec-exp
(lambda (p-names b-varss p-bodies letrec-body k-exp)
(cps-letrec-exp
p-names
(map
(lambda (b-vars) (append b-vars (list ’k%00)))
b-varss)
(map
(lambda (p-body)
(cps-of-exp p-body (cps-var-exp ’k%00)))
p-bodies)
(cps-of-exp letrec-body k-exp))))
Figure6.11 cps-of-exp,part2

224 6 Continuation-PassingStyle
cps-of-program : InpExp → TfExp
(define cps-of-program
(lambda (pgm)
(cases program pgm
(a-program (exp1)
(cps-a-program
(cps-of-exps (list exp1)
(lambda (new-args)
(simple-exp->exp (car new-args)))))))))
Figure6.12 cps-of-program
(cid:3)(cid:3)
Exercise6.27 [ ] As it stands, cps-of-let-exp will generate a useless let
expression. (Why?) Modifythis procedureso that the continuation variable is the
sameastheletvariable.Thenifexp isnonsimple,
1
(cps-of-exp <<let var = exp in exp >> K)
1 1 2
= (cps-of-exp exp <<proc (var ) (cps-of-exp exp K)>>
1 1 2
(cid:3)
Exercise6.28 [ ]Foodforthought:imagineaCPStransformerforSchemeprograms,
andimaginethatyouapplyittothefirstinterpreterfromchapter3.Whatwouldthe
resultlooklike?
(cid:3)(cid:3)
Exercise6.29 [ ]Considerthisvariantofcps-of-exps.
(define cps-of-exps
(lambda (exps builder)
(let cps-of-rest ((exps exps) (acc ’()))
cps-of-rest : Listof(InpExp) × Listof(SimpleExp) → TfExp
(cond
((null? exps) (builder (reverse acc)))
((inp-exp-simple? (car exps))
(cps-of-rest (cdr exps)
(cons
(cps-of-simple-exp (car exps))
acc)))
(else
(let ((var (fresh-identifier ’var)))
(cps-of-exp (car exps)
(cps-proc-exp (list var)
(cps-of-rest (cdr exps)
(cons
(cps-of-simple-exp (var-exp var))
acc))))))))))
Whyisthisvariantofcps-of-expmoreefficientthantheoneinfigure6.8?

6.3 ConvertingtoContinuation-PassingStyle 225
(cid:3)(cid:3)
Exercise6.30 [ ]Acalltocps-of-expswithalistofexpressionsoflengthonecan
besimplifiedasfollows:
(cps-of-exps (list exp) builder)
= (cps-of-exp/ctx exp (lambda (simp) (builder (list simp))))
where
cps-of-exp/ctx : InpExp × (SimpleExp → TfExp) → TfExp
(define cps-of-exp/ctx
(lambda (exp context)
(if (inp-exp-simple? exp)
(context (cps-of-simple-exp exp))
(let ((var (fresh-identifier ’var)))
(cps-of-exp exp
(cps-proc-exp (list var)
(context (cps-var-exp var))))))))
Thus, we can simplify occurrences of (cps-of-exps (list ...)), since the
number of arguments to list is known. Therefore the definition of, for exam-
ple, cps-of-diff-expcould be defined with cps-of-exp/ctx instead of with
cps-of-exps.
(define cps-of-diff-exp
(lambda (exp1 exp2 k-exp)
(cps-of-exp/ctx exp1
(lambda (simp1)
(cps-of-exp/ctx exp2
(lambda (simp2)
(make-send-to-cont k-exp
(cps-diff-exp simp1 simp2))))))))
Fortheuseofcps-of-expsincps-of-call-exp,wecanusecps-of-exp/ctx
on the rator, but we still need cps-of-exps for the rands. Remove all other
occurrencesofcps-of-expsfromthetranslator.
(cid:3)(cid:3)(cid:3)
Exercise6.31 [ ] Write a translator that takes the output of cps-of-program
andproducesanequivalentprograminwhichallthecontinuationsarerepresented
bydatastructures,asinchapter5. Representdatastructureslikethoseconstructed
usingdefine-datatypeaslists. Since ourlanguagedoesnothave symbols,you
canuseanintegertaginthecarpositiontodistinguishthevariantsofadatatype.
(cid:3)(cid:3)(cid:3)
Exercise6.32 [ ]Writeatranslatorliketheoneinexercise6.31,exceptthatitrep-
resentsallproceduresbydatastructures.
(cid:3)(cid:3)(cid:3)
Exercise6.33 [ ] Write a translator that takes the output from exercise6.32 and
convertsittoaregisterprogramliketheoneinfigure6.1.

226 6 Continuation-PassingStyle
(cid:3)(cid:3)
Exercise6.34 [ ]WhenweconvertaprogramtoCPS,wedomorethanproducea
programinwhichthecontrolcontextsbecomeexplicit.Wealsochoosetheexactorder
inwhichtheoperationsaredone,andchoosenamesforeachintermediateresult.The
latter is calledsequentialization. If we don’tcare about obtaining iterative behavior,
wecansequentializeaprogrambyconvertingittoA-normalformorANF.Here’san
exampleofaprograminANF.
(define fib/anf
(lambda (n)
(if (< n 2)
1
(let ((val1 (fib/anf (- n 1))))
(let ((val2 (fib/anf (- n 2))))
(+ val1 val2))))))
WhereasaprograminCPSsequentializescomputationbypassingcontinuationsthat
nameintermediateresults,aprograminANFsequentializescomputationbyusing
letexpressionsthatnamealloftheintermediateresults.
Retarget cps-of-exp so that it generates programs in ANF instead of CPS. (For
conditionalexpressionsoccurringinnontailposition,usetheideasinexercise6.23.)
Then, show that applying the revised cps-of-exp to, e.g., the definition of fib
yieldsthedefinitionoffib/anf.Finally,showthatgivenaninputprogramwhichis
alreadyinANF,yourtranslatorproducesthesameprogramexceptforthenamesof
boundvariables.
(cid:3)
Exercise6.35 [ ]Verifyonafewexamplesthatiftheoptimizationofexercise6.27is
installed,CPS-transformingtheoutputofyourANFtransformer(exercise6.34)ona
programyieldsthesameresultasCPS-transformingtheprogram.
6.4 ModelingComputationalEffects
AnotherimportantuseofCPSistoprovideamodelinwhichcomputational
effectscanbemadeexplicit. Acomputationaleffectisaneffectlikeprinting
or assigning to a variable, which is difficult to model using equational rea-
soning of the sort used inchapter 3. Bytransformingto CPS,we canmake
theseeffectsexplicit,justaswedidwithnonlocalcontrolflowinchapter5.
InusingCPStomodeleffects,our basicprinciple isthatasimple expres-
sion should have no effects. This principle underliesour rule that a simple
expressionshould havenoprocedurecalls,sinceaprocedurecallcould fail
toterminate(whichiscertainlyaneffect!).
Inthissectionwestudythreeeffects: printing, astore(usingtheexplicit-
referencemodel),andnonstandardcontrolflow.

| 6.4 ModelingComputationalEffects |                               |     |     | 227 |
| -------------------------------- | ----------------------------- | --- | --- | --- |
| Letusfirstconsiderprinting.      | Printingcertainlyhasaneffect: |     |     |     |
| (f print(3)                      | print(4))                     |     |     |     |
and
(f 1 1)
havedifferenteffects, eventhough they returnthe sameanswer. The effect
also depends on the order of evaluation of arguments; up to now our lan-
guageshave alwaysevaluated their argumentsfrom leftto right, but other
languagesmightnotdoso.
WecanmodeltheseconsiderationsbymodifyingourCPStransformation
inthefollowingways:
• WeaddtoCPS-INaprintexpression
|     | InpExp::=print | (InpExp) |     |     |
| --- | -------------- | -------- | --- | --- |
print-exp (exp1)
WehavenotwrittenaninterpreterforCPS-IN,buttheinterpreterwould
havetobeextendedsothataprint-expprintsthevalueofitsargument
andreturnssomevalue(whichwearbitrarilychoosetobe38).
• WeaddtoCPS-OUTaprintkexpression
TfExp::=printk
|     | (SimpleExp)    | ;TfExp       |       |     |
| --- | -------------- | ------------ | ----- | --- |
|     | cps-printk-exp | (simple-exp1 | body) |     |
printk(simp);exp
| The expression |     | has an effect: | it prints. Therefore | it  |
| -------------- | --- | -------------- | -------------------- | --- |
mustbeaTfExp,notaSimpleExp,andcanappearonlyintailposition. The
value of exp becomes the value of the entire printk expression, so exp
isitselfintailposition and canbeatfexp. Thuswemight writebitsof
codelike
| proc (v1) |     |     |     |     |
| --------- | --- | --- | --- | --- |
printk(-(v1,1));
| (f v1 | K)  |     |     |     |
| ----- | --- | --- | --- | --- |
Toimplementthis,weaddtotheinterpreterforCPS-OUTtheline

228 6 Continuation-PassingStyle
|     | (printk-exp |     | (simple | body) |     |     |     |
| --- | ----------- | --- | ------- | ----- | --- | --- | --- |
(begin
|     |     | (eopl:printf         |     | "~s~%"           |        |       |     |
| --- | --- | -------------------- | --- | ---------------- | ------ | ----- | --- |
|     |     | (value-of-simple-exp |     |                  | simple | env)) |     |
|     |     | (value-of/k          |     | body env cont))) |        |       |     |
• Weaddtocps-of-expalinethattranslatesfromaprintexpressionto
aprintkexpression. Wehavearbitrarilydecidedtohaveprintexpres-
| sionreturnthevalue38. |     |              |     | Soourtranslationshouldbe |               |     |            |
| --------------------- | --- | ------------ | --- | ------------------------ | ------------- | --- | ---------- |
| (cps-of-exp           |     | <<print(simp |     | )>> K)                   | = printk(simp |     | ); (K 38)) |
|                       |     |              |     | 1                        |               |     | 1          |
andweusecps-of-expstotakecareofthepossibilitythattheargument
to print is nonsimple. This gets us to a new line in cps-of-exp that
says:
|     |     | (print-exp   | (rator) |              |     |     |     |
| --- | --- | ------------ | ------- | ------------ | --- | --- | --- |
|     |     | (cps-of-exps |         | (list rator) |     |     |     |
|     |     | (lambda      |         | (simples)    |     |     |     |
(cps-printk-exp
|     |     |     | (car               | simples) |          |     |     |
| --- | --- | --- | ------------------ | -------- | -------- | --- | --- |
|     |     |     | (make-send-to-cont |          | k-exp    |     |     |
|     |     |     | (cps-const-exp     |          | 38)))))) |     |     |
Letuswatchthisworkonalargerexample.
| (cps-of-exp   |             | <<(f        | print((g | x)) print(4))>>     |     | K)       |     |
| ------------- | ----------- | ----------- | -------- | ------------------- | --- | -------- | --- |
| = (cps-of-exp |             | <<print((g  |          | x))>>               |     |          |     |
|               | <<proc      | (v1)        |          |                     |     |          |     |
|               | (cps-of-exp |             | <<(f     | v1 print(4))>>      |     | K)>>)    |     |
| = (cps-of-exp |             | <<(g        | x)>>     |                     |     |          |     |
|               | <<proc      | (v2)        |          |                     |     |          |     |
|               | (cps-of-exp |             | <<(print | v2)>>               |     |          |     |
|               |             | <<proc      | (v1)     |                     |     |          |     |
|               |             | (cps-of-exp |          | <<(f v1 print(4))>> |     | K)>>)>>) |     |
= (g x
| proc | (v2)        |      |          |                |     |      |     |
| ---- | ----------- | ---- | -------- | -------------- | --- | ---- | --- |
|      | (cps-of-exp |      | <<(print | v2)>>          |     |      |     |
|      | <<proc      | (v1) |          |                |     |      |     |
|      | (cps-of-exp |      | <<(f     | v1 print(4))>> |     | K))) |     |
= (g x
| proc | (v2) |     |     |     |     |     |     |
| ---- | ---- | --- | --- | --- | --- | --- | --- |
printk(v2);
|     | let v1         | = 38 |      |               |      |     |     |
| --- | -------------- | ---- | ---- | ------------- | ---- | --- | --- |
|     | in (cps-of-exp |      | <<(f | v1 print(4)>> | K))) |     |     |

6.4 ModelingComputationalEffects 229
= (g x
proc (v2)
printk(v2);
let v1 = 38
| in (cps-of-exp | <<print(4)>>  |        |
| -------------- | ------------- | ------ |
| <<proc (v3)    |               |        |
| (cps-of-exp    | <<(f v1 v3)>> | K)>>)) |
= (g x
proc (v2)
printk(v2);
let v1 = 38
in printk(4);
| let v3 = 38    |               |     |
| -------------- | ------------- | --- |
| in (cps-of-exp | <<(f v1 v3)>> | K)) |
= (g x
proc (v2)
printk(v2);
let v1 = 38
in printk(4);
| let v3 = 38 |     |     |
| ----------- | --- | --- |
| in (f v1 v3 | K)) |     |
Here, we call g in a continuation that names the result v2. The contin-
uation prints the value of v2 and sends 38 to the next continuation, which
binds v1 to its argument 38, prints 4 and then calls the next continuation,
whichbindsv3toitsargument(also38)andthencallsfwithv1,v3,andK.
Inthiswaythesequencingofthedifferentprintingactionsbecomesexplicit.
To model explicit references(section 4.2), we go through the same steps:
weaddnewsyntaxtoCPS-INandCPS-OUT,writenewinterpreterlinesto
interpretthenewsyntaxinCPS-OUT,andaddnewlinestocps-of-expto
translate the new syntax from CPS-IN to CPS-OUT. For explicit references,
we will need to add syntax for referencecreation, dereference, and assign-
ment.
• WeaddtoCPS-INthesyntax
InpExp::=newref
(InpExp)
|     | newref-exp | (exp1) |
| --- | ---------- | ------ |
InpExp::=deref
(InpExp)
|     | deref-exp | (exp1) |
| --- | --------- | ------ |
InpExp::=setref
(InpExp , InpExp)
|     | setref-exp | (exp1 exp2) |
| --- | ---------- | ----------- |

230 6 Continuation-PassingStyle
• WeaddtoCPS-OUTthesyntax
TfExp::=newrefk
|     |     |                 |     | (simple-exp | , simple-exp) |          |     |     |
| --- | --- | --------------- | --- | ----------- | ------------- | -------- | --- | --- |
|     |     | cps-newrefk-exp |     |             | (simple1      | simple2) |     |     |
TfExp::=derefk
|     |                 |                 | (simple-exp |             | , simple-exp) |          |         |     |
| --- | --------------- | --------------- | ----------- | ----------- | ------------- | -------- | ------- | --- |
|     |                 | cps-derefk-exp  |             |             | (simple1      | simple2) |         |     |
|     | TfExp::=setrefk |                 |             | (simple-exp | , simple-exp) |          | ; TfExp |     |
|     |                 | cps-setrefk-exp |             |             | (simple1      | simple2  | body)   |     |
A newrefk expression takes two arguments: the value to be placed in
the newly allocated cell, and a continuation to receive a referenceto the
newlocation. derefkbehavessimilarly. Sincesetrefisnormallyexe-
|       |     |              |            |     | setrefk |         | printk. |     |
| ----- | --- | ------------ | ---------- | --- | ------- | ------- | ------- | --- |
| cuted | for | effect only, | the design |     | of      | follows | that of | It  |
assigns the value of the second argument to the value of the first argu-
ment,whichshouldbeareference,andthenexecutesthethirdargument
tail-recursively.
Inthislanguagewewouldwrite
|     | newrefk(33, |     | proc        | (loc1) |      |        |     |     |
| --- | ----------- | --- | ----------- | ------ | ---- | ------ | --- | --- |
|     |             |     | newrefk(44, |        | proc | (loc2) |     |     |
setrefk(loc1,22);
|     |     |     |     |     | derefk(loc1, |     | proc (val) |     |
| --- | --- | --- | --- | --- | ------------ | --- | ---------- | --- |
-(val,1))))
This programallocatesa new location containing 33, and binds loc1 to
that location. It then allocates a new location containing 44, and binds
| loc2 |     |                |     |           |              |     | loc1        |        |
| ---- | --- | -------------- | --- | --------- | ------------ | --- | ----------- | ------ |
|      | to  | that location. | It  | then sets | the contents |     | of location | to 22. |
Finally,itdereferencesloc1,bindstheresult(whichshouldbe22)toval,
andreturnsthevalueof-(val,1),yielding21.
Togetthisbehavior,weaddtheselinestotheinterpreterforCPS-OUT.
|     | (cps-newrefk-exp |             |                      | (simple1 | simple2) |         |                |     |
| --- | ---------------- | ----------- | -------------------- | -------- | -------- | ------- | -------------- | --- |
|     |                  | (let ((val1 | (value-of-simple-exp |          |          |         | simple1 env))  |     |
|     |                  | (val2       | (value-of-simple-exp |          |          |         | simple2 env))) |     |
|     |                  | (let        | ((newval             | (ref-val |          | (newref | val1))))       |     |
(apply-procedure
|     |     | (expval->proc |         |     | val2) |     |     |     |
| --- | --- | ------------- | ------- | --- | ----- | --- | --- | --- |
|     |     | (list         | newval) |     |       |     |     |     |
k-exp))))

6.4 ModelingComputationalEffects 231
| (cps-derefk-exp |     | (simple1 | simple2) |     |     |
| --------------- | --- | -------- | -------- | --- | --- |
(apply-procedure
|     | (expval->proc | (value-of-simple-exp |     |     | simple2 env)) |
| --- | ------------- | -------------------- | --- | --- | ------------- |
(list
(deref
(expval->ref
|     | (value-of-simple-exp |     |     | simple1 | env)))) |
| --- | -------------------- | --- | --- | ------- | ------- |
k-exp))
| (cps-setrefk-exp |       | (simple1             | simple2 | body)   |                |
| ---------------- | ----- | -------------------- | ------- | ------- | -------------- |
| (let             | ((ref | (expval->ref         |         |         |                |
|                  |       | (value-of-simple-exp |         |         | simple1 env))) |
|                  | (val  | (value-of-simple-exp |         | simple2 | env)))         |
(begin
|     | (setref!    | ref  | val)          |     |     |
| --- | ----------- | ---- | ------------- | --- | --- |
|     | (value-of/k | body | env k-exp)))) |     |     |
• Finally,weaddtheselinestocps-of-exptoimplementthetranslation.
| (newref-exp  |                  | (exp1)      |               |     |           |
| ------------ | ---------------- | ----------- | ------------- | --- | --------- |
| (cps-of-exps |                  | (list       | exp1)         |     |           |
|              | (lambda          | (simples)   |               |     |           |
|              | (cps-newrefk-exp |             | (car simples) |     | k-exp)))) |
| (deref-exp   | (exp1)           |             |               |     |           |
| (cps-of-exps |                  | (list       | exp1)         |     |           |
|              | (lambda          | (simples)   |               |     |           |
|              | (cps-derefk-exp  |             | (car simples) |     | k-exp)))) |
| (setref-exp  |                  | (exp1 exp2) |               |     |           |
| (cps-of-exps |                  | (list       | exp1 exp2)    |     |           |
|              | (lambda          | (simples)   |               |     |           |
(cps-setrefk-exp
(car simples)
|     | (cadr              | simples) |          |     |     |
| --- | ------------------ | -------- | -------- | --- | --- |
|     | (make-send-to-cont |          | k-exp    |     |     |
|     | (cps-const-exp     |          | 23)))))) |     |     |
Inthelastline,wemakeitappearthatasetrefreturnsthevalueof23,
justlikeinEXPLICIT-REFS.
(cid:3)(cid:3)
Exercise6.36 [ ] Addabeginexpression(exercise4.4)toCPS-IN.Youshouldnot
needtoaddanythingtoCPS-OUT.
Exercise6.37 [ (cid:3)(cid:3)(cid:3) ] Add implicit references (section 4.3) to CPS-IN. Use the same
versionofCPS-OUT,withexplicitreferences,andmakesureyourtranslatorinserts
allocationanddereferencewherenecessary. Asahint, recallthatinthepresenceof
implicitreferences,avar-expisnolongersimple,sinceitreadsfromthestore.

| 232 |     |     | 6   | Continuation-PassingStyle |     |
| --- | --- | --- | --- | ------------------------- | --- |
(cid:3)(cid:3)(cid:3)
Exercise6.38 [ ] Ifavariableneverappearsontheleft-handsideofasetexpres-
sion,thenitisimmutable, andcouldbetreatedassimple. Extendyoursolutionto
theprecedingexercisesothatallsuchvariablesaretreatedassimple.
Finally,wecometononlocalcontrolflow. Let’sconsiderletccfromexer-
cise 5.42. A letcc expression letcc var in body binds the current con-
tinuation to the variable var. This binding is in scope in body. The only
|           |                  | throw. |                   | throw |            |
| --------- | ---------------- | ------ | ----------------- | ----- | ---------- |
| operation | on continuations | is     | We use the syntax |       | Expression |
toExpression, whichevaluatesthe twosubexpressions. Thesecond expres-
sion should returna continuation, which is appliedto the value of the first
expression. Thecurrentcontinuationofthethrowexpressionisignored.
Wefirstanalyzetheseexpressionsaccordingtotheparadigmofthischap-
letcc
ter. These expressions arenever simple. The body partof a is a tail
position,sinceitsvalueisthevalueoftheentireexpression. Sincebothposi-
tionsinathrowareevaluated,andneitheristhevalueofthethrow(indeed,
thethrowhasnovalue,sinceitneverreturnstoitsimmediatecontinuation),
theyarebothoperandpositions.
Wecannowsketchtherulesforconvertingthesetwoexpressions.
| (cps-of-exp    | <<letcc | var in body>> | K)         |     |     |
| -------------- | ------- | ------------- | ---------- | --- | --- |
| = let          | var = K |               |            |     |     |
| in (cps-of-exp |         | body var)     |            |     |     |
| (cps-of-exp    | <<throw | simp to       | simp >> K) |     |     |
|                |         | 1             | 2          |     |     |
| = (simp        | simp )  |               |            |     |     |
2 1
cps-of-exps,
| We will use                   |     | as usual,                 | to deal with | the possibility | that the |
| ----------------------------- | --- | ------------------------- | ------------ | --------------- | -------- |
| argumentstothrowarenonsimple. |     | HereKisignored,asdesired. |              |                 |          |
ForthisexamplewedonothavetoaddanysyntaxtoCPS-OUT,sincewe
arejustmanipulatingcontrolstructure.
(cid:3)
| Exercise6.39 | [ ]ImplementletccandthrowintheCPStranslator. |     |     |     |     |
| ------------ | -------------------------------------------- | --- | --- | --- | --- |
(cid:3)(cid:3)
Exercise6.40 [ ] Implement try/catch and throw from section 5.4 by adding
them to the CPS translator. You should not need to add anything to CPS-OUT.
Instead,modifycps-of-exptotaketwocontinuations: asuccesscontinuationand
anerrorcontinuation.

7
Types
We’ve seen how we canuse interpretersto model the run-time behavior of
programs.Nowwe’dliketousethesametechnologytoanalyzeorpredictthe
behaviorofprogramswithoutrunningthem.
We’vealreadyseensomeofthis: ourlexical-addresstranslatorpredictsat
analysis time where in the environment each variable will be found at run
time. Further, the actual translator looked like an interpreter, except that
insteadofpassingaroundanenvironment, we passedaroundastaticenvi-
ronment.
Our goal isto analyze aprogramto predictwhether evaluationof a pro-
gram is safe, that is, whether the evaluation will proceed without certain
kinds of errors. Exactly what is meant by safety, however, may vary from
languagetolanguage. Ifwecanguaranteethatevaluationissafe,wewillbe
surethattheprogramsatisfiesitscontract.
Inthis chapter,wewillconsider languagesthataresimilar toLETRECin
chapter3. Fortheselanguageswesaythatanevaluationissafeifandonlyif:
1. Foreveryevaluationofavariablevar,thevariableisbound.
2. Foreveryevaluationofadifferenceexpression(diff-exp exp exp ),
1 2
thevaluesofexp andexp arebothnum-vals.
1 2
3. For every evaluation of an expression of the form (zero?-exp exp ),
1
thevalueofexp isanum-val.
1
4. For every evaluation of a conditional expression (if-exp exp exp
1 2
exp ),thevalueofexp isabool-val.
3 1
5. Foreveryevaluationofaprocedurecall(call-exp rator rand),theval-
ueofratorisaproc-val.

234 7 Types
Theseconditionsassertthateachoperatorisperformedonlyonoperands
ofthecorrecttype. Wethereforecallviolationsoftheseconditionstypeerrors.
A safe evaluation may still fail for other reasons: division by zero, tak-
ing the car of an empty list, etc. We do not include these as part of our
definition of safety because predicting safety for these conditions is much
harderthanguaranteeingtheconditionslistedabove.Similarly,asafeevalu-
ationmayruninfinitely. Wedonotincludenonterminationaspartofsafety
because checking for termination is also very difficult (indeed, it is unde-
cidableingeneral). Some languageshavetypesystems thatguaranteecon-
ditions stronger than the ones above, but those are more complex than the
onesweconsiderhere.
Ourgoalistowriteaprocedurethatlooksattheprogramtextandeither
acceptsor rejectsit. Furthermore, we would like our analysis procedureto
be conservative: if the analysis accepts the program, then we can be sure
evaluation of the program will be safe. If the analysis cannot be sure that
evaluationwillbe safe,itmustrejectthe program. Inthiscase, we saythat
theanalysisissound.
Ananalysisthatrejectedeveryprogramwould still besound, sowe also
want our analysis to accept a large set of programs. The analyses in this
chapterwillacceptenoughprogramstobeuseful.
Herearesome examplesof programsthatshould berejectedor accepted
byouranalysis:
| if 3 then | 88 else 99 | reject:non-booleantest |
| --------- | ---------- | ---------------------- |
| proc (x)  | (3 x)      |                        |
reject:non-proc-valrator
| proc (x)  | (x 3)       | accept                        |
| --------- | ----------- | ----------------------------- |
| proc (f)  | proc (x) (f | x) accept                     |
| let x =   | 4 in (x 3)  | reject:non-proc-valrator      |
| (proc (x) | (x 3)       | reject:sameasprecedingexample |
4)
| let x =   | zero?(0) | reject:non-integerargumenttoadiff-exp |
| --------- | -------- | ------------------------------------- |
| in -(3,   | x)       |                                       |
| (proc (x) | -(3,x)   | reject:sameasprecedingexample         |
zero?(0))
| let f =   | 3         |                                     |
| --------- | --------- | ----------------------------------- |
| in proc   | (x) (f x) | reject:non-proc-valrator            |
| (proc (f) | proc (x)  | (f x) reject:sameasprecedingexample |
3)
| letrec | f(x) = (f -(x,-1)) | acceptnonterminatingbutsafe |
| ------ | ------------------ | --------------------------- |
in (f 1)

7.1 ValuesandTheirTypes 235
Althoughtheevaluationofthelastexampledoesnotterminate,theeval-
uation is safe by the definition given above, so our analysis is permitted to
acceptit. Asitturns out, our analysis will acceptit, becausethe analysisis
notfineenoughtodeterminethatthisprogramdoesnothalt.
7.1 ValuesandTheir Types
Since the safety conditions talk only about num-val, bool-val, and
proc-val,one might think thatit would be enough tokeep trackof these
three types. But that is not enough: if all we know is that f is bound to
a proc-val, then we can not draw any conclusions whatsoever about the
valueof(f 1). Fromthisargument,welearnthatweneedtokeeptrackof
finerinformationaboutprocedures. Thisfinerinformationiscalledthetype
structureofthelanguage.
Our languages will have a very simple type structure. For the moment,
consider the expressed values of LETREC. These values include only one-
argument procedures, but dealing with multiargument procedures, as in
exercise3.33, is straightforward: itrequiressome additionalwork butdoes
notrequireanynewideas.
GrammarforTypes
Type::=int
int-type ()
Type::=bool
bool-type ()
Type::=(Type -> Type)
proc-type (arg-type result-type)
Toseehowthistypesystemworks,let’slookatsomeexamples.

236 7 Types
Examplesofvaluesandtheirtypes
Thevalueof3hastypeint.
Thevalueof-(33,22)hastypeint.
Thevalueofzero?(11)hastypebool.
The value of proc (x) -(x,11) has type (int -> int) since, when
givenaninteger,itreturnsaninteger.
| Thevalueofproc | (x) | let y = -(x,11) | in  | -(x,y) |     |
| -------------- | --- | --------------- | --- | ------ | --- |
hastype(int -> int),sincewhengivenaninteger,itreturnsaninteger.
| Thevalueofproc | (x) | if x then | 11 else 22 |     |     |
| -------------- | --- | --------- | ---------- | --- | --- |
hastype(bool -> int),sincewhengivenaboolean,itreturnsaninteger.
| Thevalueofproc | (x) | if x then | 11 else zero?(11)hasnotypein |     |     |
| -------------- | --- | --------- | ---------------------------- | --- | --- |
ourtypesystem,sincewhengivenabooleanitmightreturneitheraninteger
oraboolean,andwehavenotypethatdescribesthisbehavior.
| Thevalueofproc | (x)      | proc (y) if | y then     | x else 11        |     |
| -------------- | -------- | ----------- | ---------- | ---------------- | --- |
| (int           | -> (bool | -> int)),   |            |                  |     |
| has type       |          |             | since when | given a boolean, | it  |
returnsaprocedurefrombooleanstointegers.
| Thevalueofproc | (f)      | if (f 3) then | 11 else                 | 22  |     |
| -------------- | -------- | ------------- | ----------------------- | --- | --- |
| hastype((int   | -> bool) | -> int),since | whengivenaprocedurefrom |     |     |
integerstobooleans,itreturnsaninteger.
| Thevalueofproc | (f) | (f 3)hastype((int | ->  | t) -> t)foranytype |     |
| -------------- | --- | ----------------- | --- | ------------------ | --- |
t, since when given a procedure of type (int -> t), it returns a value of
typet.
The value of proc (f) proc (x) (f (f x)) has type ((t -> t) ->
(t -> t))foranytypet,sincewhengivenaprocedureoftype(t -> t),it
returnsanotherprocedurethat,whengivenanargumentoftypet,returnsa
valueoftypet.

7.1 ValuesandTheirTypes 237
Wecanexplaintheseexamplesbythefollowingdefinition.
Definition7.1.1 Thepropertyofanexpressedvaluevbeingoftypetisdefinedby
inductionont:
• Anexpressedvalueisoftypeintifandonlyifitisanum-val.
• Itisoftypeboolifandonlyifitisabool-val.
→
• Itisoftype(t t )ifandonlyifitisaproc-valwiththepropertythatif
1 2
|     | itisgivenanargumentoftypet |     |     |     | ,thenoneofthefollowingthingshappens: |     |     |
| --- | -------------------------- | --- | --- | --- | ------------------------------------ | --- | --- |
1
|     | 1. itreturnsavalueoftypet |     |     |     | 2   |     |     |
| --- | ------------------------- | --- | --- | --- | --- | --- | --- |
2. itfailstoterminate
3. itfailswithanerrorotherthanatypeerror.
Weoccasionallysay“vhastypet”insteadof“visoftypet.”
This is a definition by induction on t. It depends, however, on the set of
typeerrorsbeingdefinedindependently,aswedidabove.
In this system, a value v can be of more than one type. For example,
|     |       | proc | (x) | x   | (t      | → t)    |                   |
| --- | ----- | ---- | --- | --- | ------- | ------- | ----------------- |
| the | value | of   |     | is  | of type | for any | type t. Some val- |
uesmay haveno type, like the value of proc (x) if x then 11 else
zero?(11).
Exercise7.1 [ (cid:3) ]Below is a list of closed expressions. Consider the value of each
expression. For each value, what type or types does it have? Some of the values
mayhavenotypethatisdescribableinourtypelanguage.
|     | proc | (x) -(x,3) |     |     |     |     |     |
| --- | ---- | ---------- | --- | --- | --- | --- | --- |
1.
| 2.  | proc  | (f) proc | (x)    | -((f   | x), 1)      |     |     |
| --- | ----- | -------- | ------ | ------ | ----------- | --- | --- |
| 3.  | proc  | (x) x    |        |        |             |     |     |
| 4.  | proc  | (x) proc | (y)    | (x y). |             |     |     |
| 5.  | proc  | (x) (x   | 3)     |        |             |     |     |
| 6.  | proc  | (x) (x   | x)     |        |             |     |     |
| 7.  | proc  | (x) if   | x then | 88     | else 99     |     |     |
| 8.  | proc  | (x) proc | (y)    | if x   | then y else | 99  |     |
|     | (proc | (p) if   | p then | 88     | else 99     |     |     |
9.
33)
|     | (proc | (p) if | p then | 88  | else 99 |     |     |
| --- | ----- | ------ | ------ | --- | ------- | --- | --- |
10.
|     | proc | (z) z) |     |     |     |     |     |
| --- | ---- | ------ | --- | --- | --- | --- | --- |
proc (f)
11.
|     | proc | (g) |       |        |         |              |       |
| --- | ---- | --- | ----- | ------ | ------- | ------------ | ----- |
|     | proc | (p) |       |        |         |              |       |
|     | proc | (x) | if (p | (f x)) | then (g | 1) else -((f | x),1) |

238 7 Types
| 12. | proc (x) |     |     |     |
| --- | -------- | --- | --- | --- |
proc(p)
proc (f)
|     | if (p    | x) then | -(x,1)  | else (f p) |
| --- | -------- | ------- | ------- | ---------- |
| 13. | proc (f) |         |         |            |
|     | let d    | = proc  | (x)     |            |
|     |          | proc    | (z) ((f | (x x)) z)  |
|     | in proc  | (n) ((f | (d d))  | n)         |
(cid:3)(cid:3)
Exercise7.2 [ ]Arethereanyexpressedvaluesthathaveexactlytwotypesaccord-
ingtodefinition7.1.1?
(cid:3)(cid:3)
Exercise7.3 [ ] For the language LETREC, is it decidable whether an expressed
valuevalisoftypet?
| 7.2 | Assigninga | Type | to an | Expression |
| --- | ---------- | ---- | ----- | ---------- |
Sofar,we’vedealtonlywiththetypesofexpressedvalues. Inordertoana-
lyze programs, we need to write a procedure that takes an expression and
predictsthetypeofitsvalue.
type-of
More precisely, our goal is to write a procedure which, given
anexpression(callitexp)and atypeenvironment (callittenv)mapping each
variabletoatype,assignstoexpatypetwiththepropertythat
Specificationoftype-of
Whenever exp is evaluatedin an environment in which eachvariable has a
valueofthetypespecifiedforitbytenv,oneofthefollowinghappens:
|     | • theresultingvaluehastypet,                      |     |     |     |
| --- | ------------------------------------------------- | --- | --- | --- |
|     | • theevaluationdoesnotterminate,or                |     |     |     |
|     | • theevaluationfailsonanerrorotherthanatypeerror. |     |     |     |
If we can assign an expression to a type, we say that the expression is
well-typed;otherwisewesayitisill-typedorhasnotype.
Ouranalysiswillbebasedontheprinciplethatifwecanpredictthetypes
ofthevaluesofeachofthesubexpressionsinanexpression,wecanpredict
thetypeofthevalueoftheexpression.

7.2 AssigningaTypetoanExpression 239
We’llusethisideatowritedownasetofrulesthattype-ofshouldfollow.
Assume that tenv is a type environment mapping each variable to its type.
Thenweshouldhave:
Simpletypingrules
(type-of (const-exp num) tenv) = int
(type-of (var-exp var) tenv) = tenv(var)
(type-of exp tenv) = int
1
(type-of (zero?-exp exp ) tenv) = bool
1
(type-of exp tenv) = int (type-of exp tenv) = int
1 2
(type-of (diff-exp exp exp ) tenv) = int
1 2
(type-of exp tenv) = t (type-of body [var=t ]tenv) = t
1 1 1 2
(type-of (let-exp var exp body) tenv) = t
1 2
(type-of exp tenv) = bool
1
(type-of exp tenv) = t
2
(type-of exp tenv) = t
3
(type-of (if-exp exp exp exp ) tenv) = t
1 2 3
(type-of rator tenv) = (t → t ) (type-of rand tenv) = t
1 2 1
(type-of (call-exp rator rand) tenv) = t
2
If we evaluate an expression exp of type t in a suitable environment, we
knownotonlythatitsvalueisoftypet,butwealsoknowsomethingabout
the history of thatvalue. Becausethe evaluationof exp isguaranteedto be
safe,weknowthatthevalueof exp wasconstructed onlybyoperatorsthat
arelegalfortypet.Thispointofviewwillbehelpfulwhenweconsiderdata
abstractioninmoredetailinchapter8.

240 7 Types
What about procedures? If proc(var)body has type (t → t ), then it is
1 2
intendedtobecalledonanargumentoftypet . Whenbodyisevaluated,the
1
variablevarwillbeboundtoavalueoftypet .
1
Thissuggeststhefollowingrule:
(type-of body [var=t ]tenv) = t
1 2
(type-of (proc-exp var body) tenv) = (t → t )
1 2
Thisruleissound: iftype-ofmakescorrectpredictionsaboutbody,then
itmakescorrectpredictionsabout(proc-exp var body).
There’sonlyoneproblem: ifwearetryingtocomputethetypeofaproc
expression,howarewegoingtofindthetypet fortheboundvariable?Itis
1
nowheretobefound.
Therearetwostandarddesignsforrectifyingthissituation:
• TypeChecking: Inthisapproachtheprogrammerisrequiredtosupplythe
missing information about the types of bound variables, and the type-
checker deduces the types of the other expressions and checks them for
consistency.
• TypeInference:Inthisapproachthetype-checkerattemptstoinferthetypes
of the bound variables based on how the variables are used in the pro-
gram. If the language is carefully designed, the type-checker can infer
mostorallofthesetypes.
Wewillstudyeachoftheseinturn.
(cid:3)
Exercise7.4 [ ] Using the rules of this section, write derivations, like the one on
page5,thatassigntypesforproc (x) xandproc (x) proc (y) (x y). Use
therulestoassignatleasttwotypesforeachoftheseterms. Dothevaluesofthese
expressionshavethesametypes?
7.3 CHECKED:A Type-Checked Language
Our new language will be the same asLETREC, exceptthat we require the
programmertoincludethetypesofallboundvariables. Forletrec-bound
variables, we also require the programmer to specify the result type of the
procedureaswell.

7.3 CHECKED:AType-CheckedLanguage 241
HerearesomeexampleprogramsinCHECKED.
| proc | (x : | int) | -(x,1) |     |     |     |     |
| ---- | ---- | ---- | ------ | --- | --- | --- | --- |
letrec
| int | double | (x  | : int) | = if | zero?(x)  |          |     |
| --- | ------ | --- | ------ | ---- | --------- | -------- | --- |
|     |        |     |        | then | 0         |          |     |
|     |        |     |        | else | -((double | -(x,1)), | -2) |
in double
| proc | (f : | (bool | -> int)) | proc | (n  | : int) (f | zero?(n)) |
| ---- | ---- | ----- | -------- | ---- | --- | --------- | --------- |
The result type of double is int, but the type of double itself is (int
-> int),sinceitisaprocedurethattakesanintegerandreturnsaninteger.
Todefinethesyntaxofthislanguage,wechangetheproductionsforproc
andletrecexpressions.
ChangedproductionsforCHECKED
Expression::=proc
|     |          | (Identifier | :       | Type) Expression |     |     |     |
| --- | -------- | ----------- | ------- | ---------------- | --- | --- | --- |
|     | proc-exp |             | (var ty | body)            |     |     |     |
Expression::=letrec
|     | Type | Identifier | (Identifier | :   | Type) | =Expression |     |
| --- | ---- | ---------- | ----------- | --- | ----- | ----------- | --- |
|     | in   | Expression |             |     |       |             |     |
letrec-exp
|     |     | (p-result-type |     | p-name | b-var | b-var-type |     |
| --- | --- | -------------- | --- | ------ | ----- | ---------- | --- |
p-body
letrec-body)
For a proc expression with the type of its bound variable specified, the
rulebecomes
|                  |           | (type-of | body                    | [var=t | ]tenv) | = t        |         |
| ---------------- | --------- | -------- | ----------------------- | ------ | ------ | ---------- | ------- |
|                  |           |          |                         |        | var    | res        |         |
| (type-of         | (proc-exp |          | var                     | t      | body)  | tenv) = (t | → t )   |
|                  |           |          |                         | var    |        |            | var res |
| Whataboutletrec? |           |          | Atypicalletreclookslike |        |        |            |         |
letrec
|     |     |     | t p(var:t |     | ) = | e         |     |
| --- | --- | --- | --------- | --- | --- | --------- | --- |
|     |     |     | res       | var |     | proc-body |     |
ine
letrec-body

242 7 Types
Thisexpressiondeclaresaprocedurenamed p,withformalparametervar
→
oftypet andbodye . Hencethetypeof pshouldbet t .
var proc-body var res
Each of the expressions in the letrec, e and e , must be
proc-body letrec-body
checkedinatypeenvironmentwhereeachvariableisgivenitscorrecttype.
Wecanuseourscopingrulestodeterminewhatvariablesareinscope,and
hencewhattypesshouldbeassociatedwiththem.
In e , the procedure name p is in scope. As suggested above, p is
letrec-body
→
declared to have type t t . Hence e should be checked in the
var res letrec-body
typeenvironment
= = →
tenv [p (t t )]tenv
letrec-body var res
What about e ? In e , the variable p is in scope, with type
proc-body proc-body
→
t t , and the variable var is in scope, with type t . Hence the type
var res var
environmentfore shouldbe
proc-body
= =
tenv [var t ]tenv
proc-body var letrec-body
Furthermore, in this type environment, e should have result type
proc-body
t .
res
Writingthisdownasarule,weget:
(type-of e [var=t ][p=(t → t )]tenv) = t
proc-body var var res res
(type-of e [p=(t → t )]tenv) = t
letrec-body var res
(type-of letrect p(var:t )= e ine tenv) = t
res var proc-body letrec-body
Nowwehavewrittendownalltherules,sowearereadytoimplementa
typecheckerforthislanguage.
7.3.1 TheChecker
We will need to compare types for equality. We do this with the proce-
durecheck-equal-type!,whichcomparestwotypesandreportsanerror
unlesstheyareequal. check-equal-type!takesathirdargument,which
istheexpressionthatwewillblameifthetypesareunequal.
check-equal-type! : Type × Type × Exp → Unspecified
(define check-equal-type!
(lambda (ty1 ty2 exp)
(if (not (equal? ty1 ty2))
(report-unequal-types ty1 ty2 exp))))

7.3 CHECKED:AType-CheckedLanguage 243
| report-unequal-types |                        |                    | : Type | × Type × | Exp → Unspecified |
| -------------------- | ---------------------- | ------------------ | ------ | -------- | ----------------- |
| (define              | report-unequal-types   |                    |        |          |                   |
| (lambda              |                        | (ty1 ty2           | exp)   |          |                   |
|                      | (eopl:error            | ’check-equal-type! |        |          |                   |
|                      | "Types                 | didn’t             | match: | ~s !=    | ~a in~%~a"        |
|                      | (type-to-external-form |                    |        | ty1)     |                   |
|                      | (type-to-external-form |                    |        | ty2)     |                   |
exp)))
We never use the value of a call to check-equal-type!; thus a call to
check-equal-type! is executed for effect only, like the setref expres-
sionsinsection4.2.2.
| The | procedure | report-unequal-types |     |     | uses type-to-external- |
| --- | --------- | -------------------- | --- | --- | ---------------------- |
form,whichconvertsatypebackintoalistthatiseasytoread.
| type-to-external-form |                       |         | : Type    | → List       |     |
| --------------------- | --------------------- | ------- | --------- | ------------ | --- |
| (define               | type-to-external-form |         |           |              |     |
| (lambda               |                       | (ty)    |           |              |     |
|                       | (cases                | type ty |           |              |     |
|                       | (int-type             | ()      | ’int)     |              |     |
|                       | (bool-type            |         | () ’bool) |              |     |
|                       | (proc-type            |         | (arg-type | result-type) |     |
(list
|     |     | (type-to-external-form |     |     | arg-type) |
| --- | --- | ---------------------- | --- | --- | --------- |
’->
|     |     | (type-to-external-form |     |     | result-type)))))) |
| --- | --- | ---------------------- | --- | --- | ----------------- |
Nowwe cantranscribe the rulesinto a program, just aswe didfor inter-
| pretersinchapter3. |     | Theresultisshowninfigures7.1–7.3. |     |     |     |
| ------------------ | --- | --------------------------------- | --- | --- | --- |
(cid:3)(cid:3) ]Extendthecheckertohandlemultipleletdeclarations,multiargu-
| Exercise7.5 | [   |     |     |     |     |
| ----------- | --- | --- | --- | --- | --- |
mentprocedures,andmultipleletrecdeclarations. Youwillneedtoaddtypesof
| theform(t   | 1 *                                                         | t 2 * ... | * t n -> | t)tohandlemultiargumentprocedures. |     |
| ----------- | ----------------------------------------------------------- | --------- | -------- | ---------------------------------- | --- |
| Exercise7.6 | [ (cid:3) ]Extendthecheckertohandleassignments(section4.3). |           |          |                                    |     |
(cid:3)
Exercise7.7 [ ] Changethecodeforcheckinganif-expsothatifthetestexpression
isnotaboolean,theotherexpressionsarenotchecked.Giveanexpressionforwhich
thenewversionofthecheckerbehavesdifferentlyfromtheoldversion.
(cid:3)(cid:3)
Exercise7.8 [ ] Add pairof types to the language. Say that a value is of type
pairof t * t ifandonlyifitisapairconsistingofavalueoftypet andavalue
1 2 1
oftypet .Addtothelanguagethefollowingproductions:
2

244 7 Types
Tenv = Var → Type
type-of-program : Program → Type
(define type-of-program
(lambda (pgm)
(cases program pgm
(a-program (exp1) (type-of exp1 (init-tenv))))))
type-of : Exp × Tenv → Type
(define type-of
(lambda (exp tenv)
(cases expression exp
(type-of num tenv) = int
(const-exp (num) (int-type))
(type-of var tenv) = tenv(var)
(var-exp (var) (apply-tenv tenv var))
(type-of e tenv) = int (type-of e tenv) = int
1 2
(type-of (diff-exp e e ) tenv) = int
1 2
(diff-exp (exp1 exp2)
(let ((ty1 (type-of exp1 tenv))
(ty2 (type-of exp2 tenv)))
(check-equal-type! ty1 (int-type) exp1)
(check-equal-type! ty2 (int-type) exp2)
(int-type)))
(type-of e tenv) = int
1
(type-of (zero?-exp e ) tenv) = bool
1
(zero?-exp (exp1)
(let ((ty1 (type-of exp1 tenv)))
(check-equal-type! ty1 (int-type) exp1)
(bool-type)))
(type-of e tenv) = bool
1
(type-of e tenv) = t
2
(type-of e tenv) = t
3
(type-of (if-exp e e e ) tenv) = t
1 2 3
(if-exp (exp1 exp2 exp3)
(let ((ty1 (type-of exp1 tenv))
(ty2 (type-of exp2 tenv))
(ty3 (type-of exp3 tenv)))
(check-equal-type! ty1 (bool-type) exp1)
(check-equal-type! ty2 ty3 exp)
ty2))
Figure7.1 type-offorCHECKED

7.3 CHECKED:AType-CheckedLanguage 245
(type-of e tenv) = t (type-of body [var=t ]tenv) = t
1 1 1 2
(type-of (let-exp var e body) tenv) = t
1 2
(let-exp (var exp1 body)
(let ((exp1-type (type-of exp1 tenv)))
(type-of body
(extend-tenv var exp1-type tenv))))
(type-of body [var=t ]tenv) = t
var res
(type-of (proc-exp var t body) tenv) = (t → t )
var var res
(proc-exp (var var-type body)
(let ((result-type
(type-of body
(extend-tenv var var-type tenv))))
(proc-type var-type result-type)))
(type-of rator tenv) = (t → t ) (type-of rand tenv) = t
1 2 1
(type-of (call-exp rator rand) tenv) = t
2
(call-exp (rator rand)
(let ((rator-type (type-of rator tenv))
(rand-type (type-of rand tenv)))
(cases type rator-type
(proc-type (arg-type result-type)
(begin
(check-equal-type! arg-type rand-type rand)
result-type))
(else
(report-rator-not-a-proc-type
rator-type rator)))))
Figure7.2 type-offorCHECKED,cont’d.
Type ::=pairof Type * Type
pair-type (ty1 ty2)
Expression::=newpair (Expression , Expression)
pair-exp (exp1 exp2)
Expression::= unpair Identifier Identifier = Expression
in Expression
unpair-exp (var1 var2 exp body)

246 7 Types
(type-of e [var=t ][p=(t → t )]tenv) = t
proc-body var var res res
(type-of e [p=(t → t )]tenv) = t
letrec-body var res
(type-of letrect p(var:t )= e ine tenv) = t
res var proc-body letrec-body
(letrec-exp (p-result-type p-name b-var b-var-type
p-body letrec-body)
(let ((tenv-for-letrec-body
(extend-tenv p-name
(proc-type b-var-type p-result-type)
tenv)))
(let ((p-body-type
(type-of p-body
(extend-tenv b-var b-var-type
tenv-for-letrec-body))))
(check-equal-type!
p-body-type p-result-type p-body)
(type-of letrec-body tenv-for-letrec-body)))))))
Figure7.3 type-offorCHECKED,cont’d.
Apairexpressioncreatesapair;anunpairexpression(likeexercise3.18)bindsits
twovariablestothetwopartsoftheexpression;thescopeofthesevariablesisbody.
Thetypingrulesforpairandunpairare:
(type-of e tenv) = t
1 1
(type-of e tenv) = t
2 2
(type-of (pair-exp e 1 e 2 ) tenv) = pairof t 1 * t 2
(type-of e tenv) = (pairof t t )
pair 1 2
(type-of e [var =t ][var =t ]tenv) = t
body 1 1 2 2 body
(type-of (unpair-exp var var e e ) tenv) = t
1 2 1 body body
Extend CHECKED to implement these rules. In type-to-external-form, pro-
ducethelist(pairof t t )forapairtype.
1 2

7.3 CHECKED:AType-CheckedLanguage 247
(cid:3)(cid:3)
Exercise7.9 [ ]Addlistoftypestothelanguage,withoperationssimilartothose
of exercise3.9. A value is of type listof t if and only if it is a listand all of its
elementsareoftypet.Extendthelanguagewiththeproductions
::=listof
| Type              |     |             | Type  |                |     |
| ----------------- | --- | ----------- | ----- | -------------- | --- |
|                   |     | list-type   | (ty)  |                |     |
| Expression::=list |     |             |       | {,Expression}∗ |     |
|                   |     | (Expression |       |                | )   |
|                   |     | list-exp    | (exp1 | exps)          |     |
Expression::=cons
|     |     | (Expression |       | ,     | Expression) |
| --- | --- | ----------- | ----- | ----- | ----------- |
|     |     | cons-exp    | (exp1 | exp2) |             |
Expression::=null?
(Expression)
|     |     | null-exp | (exp1) |     |     |
| --- | --- | -------- | ------ | --- | --- |
Expression::=emptylist_Type
|     |     | emptylist-exp |     | (ty) |     |
| --- | --- | ------------- | --- | ---- | --- |
withtypesgivenbythefollowingfourrules:
|     | (type-of |     | e 1 tenv) | =   | t   |
| --- | -------- | --- | --------- | --- | --- |
|     | (type-of |     | e tenv)   | =   | t   |
2
.
.
.
|                    | (type-of |           | en tenv) | =     | t                |
| ------------------ | -------- | --------- | -------- | ----- | ---------------- |
| (type-of (list-exp |          | e         | (e ...   | en )) | tenv) = listof t |
|                    |          | 1         | 2        |       |                  |
| (type-of           |          | e 1 tenv) | =        | t     |                  |
| (type-of           | e        | tenv) =   | listof   | t     |                  |
2
| (type-of cons(e |     | ,e ) tenv) | =   | listof | t   |
| --------------- | --- | ---------- | --- | ------ | --- |
1 2
| (type-of         | e 1 tenv) | = listof |        | t   |     |
| ---------------- | --------- | -------- | ------ | --- | --- |
| (type-of null?(e |           | ) tenv)  | = bool |     |     |
1
| (type-of emptylist[t] |     |     | tenv) | = listof | t   |
| --------------------- | --- | --- | ----- | -------- | --- |
Althoughconsissimilartopair,ithasaverydifferenttypingrule.
Writesimilarrulesforcarandcdr,andextendthecheckertohandletheseaswell
astheotherexpressions.Useatricksimilartotheoneinexercise7.8toavoidconflict
withproc-type-exp. Theserulesshouldguaranteethatcarandcdrareapplied
tolists,buttheyshouldnotguaranteethatthelistsbenon-empty. Whywoulditbe
unreasonablefortherulestoguaranteethatthelistsbenon-empty? Whyisthetype
parameterinemptylistnecessary?

248 7 Types
(cid:3)(cid:3)
Exercise7.10 [ ]ExtendthecheckertohandleEXPLICIT-REFS.Youwillneedtodo
thefollowing:
Addtothetypesystemthetypesrefto
| •   |     |     | t,wheretisanytype.Thisisthetypeof |     |     |
| --- | --- | --- | --------------------------------- | --- | --- |
referencestolocationscontainingavalueoftypet.Thus,ifeisoftypet,(newref
| e)isoftyperefto |     | t.  |     |     |     |
| --------------- | --- | --- | --- | --- | --- |
Addtothetypesystemthetypevoid.
| •   |     |     | Thisisthetypeofthevaluereturnedby |     |     |
| --- | --- | --- | --------------------------------- | --- | --- |
setref. You can’t apply any operation to a value of type void, so it doesn’t
matter what value setref returns. This is an example of types serving as an
information-hidingmechanism.
| • Writedowntypingrulesfornewref,deref,andsetref. |     |     |     |     |     |
| ------------------------------------------------ | --- | --- | --- | --- | --- |
| • Implementtheserulesinthechecker.               |     |     |     |     |     |
(cid:3)(cid:3)
| Exercise7.11           | [ ]ExtendthecheckertohandleMUTABLE-PAIRS. |     |                     |     |     |
| ---------------------- | ----------------------------------------- | --- | ------------------- | --- | --- |
| 7.4 INFERRED:ALanguage |                                           |     | with Type Inference |     |     |
Writingdownthetypesintheprogrammaybehelpfulfordesignanddoc-
umentation, but it can be time-consuming. Another design is to have the
compiler figure out the types of all the variables, based on observing how
theyareused, andutilizinganyhintstheprogrammermightgive. Surpris-
ingly, for a carefully designed language, the compiler can always infer the
typesofthevariables. Thisstrategyiscalledtypeinference. Wecandoitfor
languageslikeLETREC,anditscalesuptoreasonably-sizedlanguages.
For our case study in type inference, we start with the language of
CHECKED. We then change the language so that all the type expressions
are optional. In place of a missing type expression, we use the marker ?.
Henceatypicalprogramlookslike
letrec
|     | ? foo (x | : ?) = if | zero?(x)           |     |     |
| --- | -------- | --------- | ------------------ | --- | --- |
|     |          | then      | 1                  |     |     |
|     |          | else      | -(x, (foo -(x,1))) |     |     |
in foo
Eachquestionmark(except,ofcourse,fortheoneattheendofzero?)indi-
catesaplacewhereatypeexpressionmustbeinferred.
Sincethetypeexpressionsareoptional,wemayfillinsomeofthe?’swith
types,asin
letrec
|     | ? even   | (x : int) = | if zero?(x) then | 1 else (odd  | -(x,1)) |
| --- | -------- | ----------- | ---------------- | ------------ | ------- |
|     | bool odd | (x : ?) =   | if zero?(x) then | 0 else (even | -(x,1)) |
in (odd 13)

7.4 INFERRED:ALanguagewithTypeInference 249
To specify this syntax, we add a new nonterminal, Optional-type, and we
modifytheproductionsforprocandletrectouseoptionaltypesinstead
oftypes.
Optional-type::=?
| no-type | ()  |     |     |
| ------- | --- | --- | --- |
Optional-type::=Type
| a-type              | (ty)             |     |     |
| ------------------- | ---------------- | --- | --- |
| ::=proc (Identifier | : Optional-type) |     |     |
Expression Expression
| proc-exp | (var otype | body) |     |
| -------- | ---------- | ----- | --- |
::= letrec
Expression
| Optional-type | Identifier | (Identifier : Optional-type) | = Expression |
| ------------- | ---------- | ---------------------------- | ------------ |
in Expression
letrec-exp
| (p-result-otype |             | p-name |     |
| --------------- | ----------- | ------ | --- |
| b-var           | b-var-otype | p-body |     |
letrec-body)
Theomittedtypeswillbetreatedasunknownsthatweneedtofind.Wedo
thisbytraversingtheabstractsyntaxtreeandgeneratingequationsbetween
thesetypes,possiblyincludingtheseunknowns. Wethensolvetheequations
fortheunknowntypes.
To see how this works, we need namesfor the unknown types. For each
expressioneorboundvariablevar,lett ort denotethetypeoftheexpres-
e var
sionorboundvariable.
Foreachnode inthe abstractsyntax treeofthe expression,the typerules
dictatesome equations thatmust hold betweenthese types. For our PROC
language,theequationswouldbe:
=int
| (diff-exp | e e ) : | t   |     |
| --------- | ------- | --- | --- |
|           | 1 2     | e1  |     |
=int
t e2
t =int
(diff-exp e1 e2 )
| (zero?-exp | e ) : | t =int |     |
| ---------- | ----- | ------ | --- |
|            | 1     | e1     |     |
=bool
t
(zero?-exp e1 )
| (if-exp | e e e ) : | t =bool |     |
| ------- | --------- | ------- | --- |
|         | 1 2 3     | e1      |     |
=
t t
|     |     | e2 (if-exp e1 e2 e3 ) |     |
| --- | --- | --------------------- | --- |
=
|     |     | t e3 t (if-exp e1 e2 e3 ) |     |
| --- | --- | ------------------------- | --- |

250 7 Types
|           |           |       |               |      |           | = →    |             |     |
| --------- | --------- | ----- | ------------- | ---- | --------- | ------ | ----------- | --- |
| (proc-exp | var body) |       | : t (proc-exp | var  | body)     | (t var | t body      | )   |
|           |           |       | =             |      | →         |        |             |     |
| (call-exp | rator     | rand) | : t           | (t   | t         |        |             | )   |
|           |           |       | rator         | rand | (call-exp |        | rator rand) |     |
• Thefirstrulesaysthattheargumentsandtheresultofadiff-expmust
allbeoftypeint.
• Thesecondrulesaysthattheargumentofazero?-expmustbeanint,
anditsresultisabool.
• Thethirdrulesaysthatinanifexpression,thetestmustbeoftypebool,
andthatthetypesofthetwoalternativesmustbethesameasthetypeof
theentireifexpression.
• Thefourthrulesaysthatthetypeofaprocexpressionisthatofaproce-
durewhoseargumenttypeisgivenbythetypeofitsboundvariable,and
whoseresulttypeisgivenbythetypeofitsbody.
• The fifth rule says that in a procedure call, the operator must have the
typeofaprocedurethatacceptsargumentsofthesametypeasthatofthe
operand,andthatproducesresultsofthesametypeasthatofthecalling
expression.
To infer the type of an expression, we’ll introduce a type variable for
everysubexpressionandeverybound variable,generatetheconstraintsfor
eachsubexpression,andthensolvetheresultingequations. Toseehowthis
works,wewillinferthetypesofseveralsampleexpressions.
| Letusstartwiththeexpressionproc |     |     | (f) | proc(x) |     | -((f | 3),(f | x)). |
| ------------------------------- | --- | --- | --- | ------- | --- | ---- | ----- | ---- |
We begin by making a table of all the bound variables, proc expressions,
ifexpressions,andprocedurecallsinthisexpression,andassigningatype
variabletoeachone.
| Expression |     |     |     |     | TypeVariable |     |     |     |
| ---------- | --- | --- | --- | --- | ------------ | --- | --- | --- |
| f          |     |     |     |     | t            |     |     |     |
f
x
t x
| proc(f)proc(x)-((f |     | 3),(f | x)) |     | t   |     |     |     |
| ------------------ | --- | ----- | --- | --- | --- | --- | --- | --- |
0
| proc(x)-((f | 3),(f | x)) |     |     | t   |     |     |     |
| ----------- | ----- | --- | --- | --- | --- | --- | --- | --- |
1
| -((f 3),(f | x)) |     |     |     | t   |     |     |     |
| ---------- | --- | --- | --- | --- | --- | --- | --- | --- |
2
| (f 3) |     |     |     |     | t   |     |     |     |
| ----- | --- | --- | --- | --- | --- | --- | --- | --- |
3
(f x)
t 4

7.4 INFERRED:ALanguagewithTypeInference 251
Now,foreachcompoundexpression,wecanwritedownatypeequation
accordingtotherulesabove.
| Expression         |           | Equations |         |
| ------------------ | --------- | --------- | ------- |
| proc(f)proc(x)-((f | 3),(f x)) | =         | →       |
|                    |           | 1. t 0    | t f t 1 |
| proc(x)-((f        | 3),(f x)) | 2. t =    | t → t   |
|                    |           | 1         | x 2     |
| -((f 3),(f         | x))       | 3. t =    | int     |
3
|     |     | 4. t = | int |
| --- | --- | ------ | --- |
4
|     |     | 5. t = | int |
| --- | --- | ------ | --- |
2
| (f 3) |     | =      | int → |
| ----- | --- | ------ | ----- |
|       |     | 6. t f | t 3   |
| (f x) |     | 7. t = | t → t |
|       |     | f      | x 4   |
• Equation1saysthattheentireexpressionproducesaprocedurethattakes
anargumentof type t andproducesa valueof the same type asthatof
f
| proc(x)-((f | 3),(f x)). |     |     |
| ----------- | ---------- | --- | --- |
• Equation 2 says that proc(x)-((f 3),(f x)) produces a procedure
thattakesanargumentoftype t x andproducesavalueofthe sametype
| asthatof-((f | 3),(f x)). |     |     |
| ------------ | ---------- | --- | --- |
• Equations3–5saythattheargumentsandtheresultofthesubtractionin
| -((f 3),(f | x))areallintegers. |     |     |
| ---------- | ------------------ | --- | --- |
• Equation 6 says that f expects an argument of type int and returns a
| valueofthesametypeasthatof(f | 3). |     |     |
| ---------------------------- | --- | --- | --- |
• Similarlyequation7saysthatfexpectsanargumentofthesametypeas
| thatofxandreturnsavalueofthesametypeasthatof(f |     |     | x). |
| ---------------------------------------------- | --- | --- | --- |
Wecanfillint , t x , t 0 , t 1 , t 2 , t 3 ,andt 4 inanywaywelike,solongasthey
f
satisfytheequations
= →
| t 0 t t 1 |     |     |     |
| --------- | --- | --- | --- |
f
t = t → t
| 1 x 2 |     |     |     |
| ----- | --- | --- | --- |
=int
t
3
t =int
4
=int
t
2
=int →
| t f t | 3   |     |     |
| ----- | --- | --- | --- |
t = t → t
f x 4
Ourgoalistofindvaluesforthevariablesthatmakealltheequationstrue.
Wecanexpresssuchasolutionasasetofequationswheretheleft-handsides

252 7 Types
areallvariables. Wecallsuchasetofequationsasubstitution. Thevariables
thatoccurontheleft-handsideofsomeequationinthesubstitutionaresaid
tobeboundinthesubstitution.
Wecansolvesuchequationssystematically. Thisprocessiscalledunifica-
tion.
Weseparatethestateofourcalculationintothesetofequationsstilltobe
solvedandthesubstitutionfoundsofar. Initially,alloftheequationsareto
besolved,andthesubstitutionfoundisempty.
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
= →
| t t | t   |     |     |
| --- | --- | --- | --- |
| 0 f | 1   |     |     |
= →
| t 1 t x | t 2 |     |     |
| ------- | --- | --- | --- |
t =int
3
=int
t
4
t =int
2
| =int | →   |     |     |
| ---- | --- | --- | --- |
| t    | t   |     |     |
| f    | 3   |     |     |
= →
| t f t x | t 4 |     |     |
| ------- | --- | --- | --- |
We consider each equation in turn. If the equation’s left-hand side is a
variable,wemoveittothesubstitution.
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
| =         | →   | =            | →   |
| t 1 t x   | t 2 | t 0 t f      | t 1 |
=int
t
3
=int
t 4
t =int
2
| =int    | →   |     |     |
| ------- | --- | --- | --- |
| t       | t   |     |     |
| f       | 3   |     |     |
| t = t → | t   |     |     |
| f x     | 4   |     |     |
However,doing thismaychange the substitution. For example,our next
equationgivesavaluefort . Weneedtopropagatethatinformationintothe
1
value for t , which contains t on its right-hand side. So we substitute the
| 0   |     | 1   |     |
| --- | --- | --- | --- |
right-handsideforeachoccurrenceoft 1 inthesubstitution. Thisgetsus:
| Equations |     | Substitution |        |
| --------- | --- | ------------ | ------ |
| =int      |     | =            | → →    |
| t         |     | t t          | (t t ) |
| 3         |     | 0 f          | x 2    |
| t =int    |     | t = t        | → t    |
| 4         |     | 1 x          | 2      |
=int
t
2
| =int  | →   |     |     |
| ----- | --- | --- | --- |
| t f   | t 3 |     |     |
| t = t | → t |     |     |
| f x   | 4   |     |     |
If the right-hand side were a variable, we’d switch the sides and do the
samething. Wecancontinueinthismannerforthenextthreeequations.

7.4 INFERRED:ALanguagewithTypeInference 253
| Equations |     | Substitution |      |     |
| --------- | --- | ------------ | ---- | --- |
| t =int    |     | t = t →      | (t → | t ) |
| 4         |     | 0 f          | x    | 2   |
| =int      |     | = →          |      |     |
| t         |     | t t x        | t    |     |
| 2         |     | 1            | 2    |     |
| t =int    | → t | t =int       |      |     |
| f         | 3   | 3            |      |     |
= →
| t t       | t   |              |     |     |
| --------- | --- | ------------ | --- | --- |
| f x       | 4   |              |     |     |
| Equations |     | Substitution |     |     |
| =int      |     | = →          | →   |     |
| t         |     | t t          | (t  | t ) |
| 2         |     | 0 f          | x   | 2   |
| t =int    | → t | t = t →      | t   |     |
| f         | 3   | 1 x          | 2   |     |
| =         | →   | =int         |     |     |
| t t       | t   | t            |     |     |
| f x       | 4   | 3            |     |     |
=int
t 4
| Equations |     | Substitution |      |      |
| --------- | --- | ------------ | ---- | ---- |
| t =int    | → t | t = t →      | (t → | int) |
| f         | 3   | 0 f          | x    |      |
| =         | →   | = →          | int  |      |
| t t       | t   | t t          |      |      |
| f x       | 4   | 1 x          |      |      |
=int
t 3
t =int
4
=int
t
2
Now, the next equation to be considered contains t , which is already
3
boundtointinthesubstitution. Sowesubstituteintfort intheequation.
3
Wewoulddothesamethingforanyothertypevariablesintheequation. We
callthisapplyingthesubstitutiontotheequation.
| Equations |       | Substitution |      |      |
| --------- | ----- | ------------ | ---- | ---- |
| =int      | → int | = →          | →    | int) |
| t f       |       | t 0 t f      | (t x |      |
| t = t →   | t     | t = t →      | int  |      |
| f x       | 4     | 1 x          |      |      |
=int
t
3
t =int
4
=int
t
2
Wemovetheresultingequationintothesubstitutionandupdatethesub-
stitutionasnecessary.
| Equations |     | Substitution |        |          |
| --------- | --- | ------------ | ------ | -------- |
| =         | →   | = (int       | → int) | → → int) |
| t f t x   | t 4 | t 0          |        | (t x     |
|           |     | t = t →      | int    |          |
1 x
=int
t
3
t =int
4
=int
t
2
|     |     | =int | → int |     |
| --- | --- | ---- | ----- | --- |
t f

254 7 Types
|     | =   | →   |     |     |     |
| --- | --- | --- | --- | --- | --- |
Thenextequation,t t x t 4 ,containst andt 4 ,whichareboundinthe
|                                                      | f       |              | f      |        |          |
| ---------------------------------------------------- | ------- | ------------ | ------ | ------ | -------- |
| substitution,soweapplythesubstitutiontothisequation. |         |              |        |        | Thisgets |
| Equations                                            |         | Substitution |        |        |          |
| int→ int=                                            | t → int | t = (int     | → int) | → (t → | int)     |
|                                                      | x       | 0            |        | x      |          |
|                                                      |         | = →          | int    |        |          |
t t
1 x
=int
t 3
=int
t
4
=int
t 2
|     |     | t =int | → int |     |     |
| --- | --- | ------ | ----- | --- | --- |
f
Ifneithersideofthe equationisavariable,wecansimplify, yieldingtwo
newequations.
| Equations |     | Substitution |          |      |      |
| --------- | --- | ------------ | -------- | ---- | ---- |
| int= t    |     | t = (int     | → int) → | (t → | int) |
| x         |     | 0            |          | x    |      |
| int=int   |     | = →          | int      |      |      |
t t x
1
t =int
3
=int
t
4
=int
t 2
|     |     | =int | →   |     |     |
| --- | --- | ---- | --- | --- | --- |
|     |     | t    | int |     |     |
f
We can process these as usual: we switch the sides of the first equation,
addittothesubstitution,andupdatethesubstitution,aswedidbefore.
| Equations |     | Substitution |        |        |        |
| --------- | --- | ------------ | ------ | ------ | ------ |
| int=int   |     | = (int       | → int) | → (int | → int) |
t
0
|     |     | =int | → int |     |     |
| --- | --- | ---- | ----- | --- | --- |
t 1
t =int
3
=int
t
4
t =int
2
|     |     | =int | →int |     |     |
| --- | --- | ---- | ---- | --- | --- |
t
f
=int
t x
Thefinalequation,int=int,isalwaystrue,sowecandiscardit.
| Equations |     | Substitution |        |        |        |
| --------- | --- | ------------ | ------ | ------ | ------ |
|           |     | = (int       | → int) | → (int | → int) |
t
0
|     |     | t =int | → int |     |     |
| --- | --- | ------ | ----- | --- | --- |
1
=int
t
3
=int
t 4
t =int
2
|     |     | =int | → int |     |     |
| --- | --- | ---- | ----- | --- | --- |
t
f
t =int
x

| 7.4 INFERRED:ALanguagewithTypeInference |     |     |     |     |     | 255 |
| --------------------------------------- | --- | --- | --- | --- | --- | --- |
We have no more equations, so we are done. We conclude fromthis cal-
culation that our original expression proc (f) proc (x) -((f 3),(f
x))shouldbeassignedthetype
| ((int → int) | → (int | → int)) |     |     |     |     |
| ------------ | ------ | ------- | --- | --- | --- | --- |
This is reasonable: The first argument f must take an int argument
because it is given 3 as an argument. It must produce an int, because its
valueisusedasanargumenttothesubtractionoperator. Andxmustbean
int,becauseitisalsosuppliedasanargumenttof.
|                 |         |          | proc(f)(f | 11). |                 |     |
| --------------- | ------- | -------- | --------- | ---- | --------------- | --- |
| Let us consider | another | example: |           |      | Again, we start | by  |
assigningtypevariables:
| Expression |     | TypeVariable |     |     |     |     |
| ---------- | --- | ------------ | --- | --- | --- | --- |
| f          |     | t            |     |     |     |     |
f
| proc(f)(f | 11) | t   |     |     |     |     |
| --------- | --- | --- | --- | --- | --- | --- |
0
(f 11)
t 1
Nextwewritedowntheequations
| Expression |     | Equations |       |     |     |     |
| ---------- | --- | --------- | ----- | --- | --- | --- |
| proc(f)(f  | 11) | t =       | t → t |     |     |     |
|            |     | 0         | f 1   |     |     |     |
→
| (f 11) |     | t = | int | t   |     |     |
| ------ | --- | --- | --- | --- | --- | --- |
|        |     | f   |     | 1   |     |     |
Andnextwesolve:
| Equations                    |     |     | Substitution |                        |     |     |
| ---------------------------- | --- | --- | ------------ | ---------------------- | --- | --- |
| =                            | →   |     |              |                        |     |     |
| t t                          | t   |     |              |                        |     |     |
| 0 f                          | 1   |     |              |                        |     |     |
| =int                         | →   |     |              |                        |     |     |
| t f                          | t 1 |     |              |                        |     |     |
| Equations                    |     |     | Substitution |                        |     |     |
| =int                         | →   |     | = →          |                        |     |     |
| t                            | t   |     | t t          | t                      |     |     |
| f                            | 1   |     | 0 f          | 1                      |     |     |
| Equations                    |     |     | Substitution |                        |     |     |
|                              |     |     | = (int       | → →                    |     |     |
|                              |     |     | t 0          | t 1 ) t                | 1   |     |
|                              |     |     | t =int       | → t                    |     |     |
|                              |     |     | f            | 1                      |     |     |
| Thismeansthatwecanassignproc |     |     |              | (f) (f 11)thetype(int→ |     | )   |
t 1
→ t ,foranychoiceoft .Again,thisisreasonable:wecaninferthatfmust
| 1   |     | 1   |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- |
beabletotakeanintargument,butwehavenoinformationabouttheresult
| typeoff,andindeedforanyt |     |     | ,thiscodewillworkforanyfthattakesan |     |     |     |
| ------------------------ | --- | --- | ----------------------------------- | --- | --- | --- |
1
intargumentandreturnsavalueoftypet . Wesayitispolymorphicint .
|           |         |          |          | 1         |             | 1   |
| --------- | ------- | -------- | -------- | --------- | ----------- | --- |
|           |         |          |          | if x then | -(x,1) else | 0.  |
| Let’s try | a third | example. | Consider |           |             |     |
Again,let’sassigntypevariablestoeachsubexpressionthatisnotaconstant.

256 7 Types
| Expression |     | TypeVariable |     |
| ---------- | --- | ------------ | --- |
| x          |     | t            |     |
x
| if x then | -(x,1) else | 0 t |     |
| --------- | ----------- | --- | --- |
0
| -(x,1) |     | t   |     |
| ------ | --- | --- | --- |
1
Wethengeneratetheequations
| Expression |             | Equations |        |
| ---------- | ----------- | --------- | ------ |
| if x then  | -(x,1) else | 0         | = bool |
t x
|     |     | t   | = t |
| --- | --- | --- | --- |
|     |     | 1   | 0   |
|     |     | int | = t |
0
| -(x,1) |     | t   | = int |
| ------ | --- | --- | ----- |
x
|     |     | t   | = int |
| --- | --- | --- | ----- |
1
Processingtheseequationsaswedidbefore,weget
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
t =bool
x
=
t t
1 0
int= t
0
=int
t
x
=int
t 1
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
| =         |     | =bool        |     |
| t t       |     | t x          |     |
1 0
int= t
0
=int
t
x
=int
t 1
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
| int=      |     | =bool        |     |
| t 0       |     | t x          |     |
| t =int    |     | t = t        |     |
| x         |     | 1 0          |     |
=int
t
1
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
| =int      |     | =bool        |     |
| t         |     | t            |     |
| 0         |     | x            |     |
| t =int    |     | t = t        |     |
| x         |     | 1 0          |     |
=int
t
1
| Equations |     | Substitution |     |
| --------- | --- | ------------ | --- |
| t =int    |     | t =bool      |     |
| x         |     | x            |     |
| =int      |     | =int         |     |
| t         |     | t            |     |
| 1         |     | 1            |     |
t =int
0

7.4 INFERRED:ALanguagewithTypeInference 257
Sincet x isalreadyboundinthesubstitution, weapplythecurrentsubsti-
tutiontothenextequation,getting
| Equations |     | Substitution |       |     |
| --------- | --- | ------------ | ----- | --- |
| bool=int  |     |              | =bool |     |
t x
| t =int |     | t   | =int |     |
| ------ | --- | --- | ---- | --- |
| 1      |     |     | 1    |     |
=int
t
0
What has happened here? We have inferred from these equations that
bool = int.Soinanysolutionoftheseequations,bool = int.Butbool
andintcannotbe equal. Thereforethereisnosolution tothese equations.
Thereforeitisimpossible toassignatypetothisexpression. Thisisreason-
able, since the expression if x then -(x,1) else 0 uses x as both a
booleanandaninteger,whichisillegalinourtypesystem.
Letus do one more example. Consider proc (f) zero?((f f)). We
proceedasbefore.
| Expression |          |     | TypeVariable |     |
| ---------- | -------- | --- | ------------ | --- |
| proc (f)   | zero?((f | f)) | t            |     |
0
| f        |     |     | t f |     |
| -------- | --- | --- | --- | --- |
| zero?((f | f)) |     | t   |     |
1
| (f f) |     |     | t   |     |
| ----- | --- | --- | --- | --- |
2
| Expression |     |     | Equations |     |
| ---------- | --- | --- | --------- | --- |
→
| proc (f) | zero?((f | f)) | t 0 | = t t 1 |
| -------- | -------- | --- | --- | ------- |
f
| zero?((f | f)) |     | t   | = bool |
| -------- | --- | --- | --- | ------ |
1
|     |     |     | t   | = int |
| --- | --- | --- | --- | ----- |
2
| (f f) |     |     | t   | = t → t |
| ----- | --- | --- | --- | ------- |
|       |     |     | f   | f 2     |
Andwesolveasusual:
| Equations |     | Substitution |     |     |
| --------- | --- | ------------ | --- | --- |
= →
| t t | t   |     |     |     |
| --- | --- | --- | --- | --- |
| 0 f | 1   |     |     |     |
=bool
t 1
t =int
2
= →
| t t       | t   |              |       |     |
| --------- | --- | ------------ | ----- | --- |
| f f       | 2   |              |       |     |
| Equations |     | Substitution |       |     |
| =bool     |     |              | =     | →   |
| t 1       |     | t            | 0 t f | t 1 |
t =int
2
= →
| t t | t   |     |     |     |
| --- | --- | --- | --- | --- |
| f f | 2   |     |     |     |

258 7 Types
| Equations |     | Substitution |     |     |
| --------- | --- | ------------ | --- | --- |
| t =int    |     | t = t → bool |     |     |
| 2         |     | 0 f          |     |     |
| =         | →   | =bool        |     |     |
| t t       | t   | t            |     |     |
| f f       | 2   | 1            |     |     |
| Equations |     | Substitution |     |     |
| =         | →   | = → bool     |     |     |
| t f t f   | t 2 | t 0 t f      |     |     |
t =bool
1
=int
t
2
| Equations |       | Substitution |     |     |
| --------- | ----- | ------------ | --- | --- |
| =         | → int | = → bool     |     |     |
| t f t f   |       | t 0 t f      |     |     |
t =bool
1
=int
t
2
| Nowwehaveaproblem.We’venowinferredthatt |     |     | = t → | Int. Butthere |
| --------------------------------------- | --- | --- | ----- | ------------- |
f f
isnotypewiththisproperty,becausetheright-handsideofthisequationis
alwayslargerthantheleft: Ifthesyntaxtreefort f containsknodes,thenthe
| right-handsidewillalwayscontaink |     | + 2nodes. |     |     |
| -------------------------------- | --- | --------- | --- | --- |
=
| Soifweeverdeduceanequationoftheformtv |     |     | twherethetypevariable |     |
| ------------------------------------- | --- | --- | --------------------- | --- |
tv occurs in the type t, we must again conclude that there is no solution to
| theoriginalequations. | Thisextraconditioniscalledtheoccurrencecheck. |     |     |     |
| --------------------- | --------------------------------------------- | --- | --- | --- |
This condition alsomeans that the substitutions we build will satisfy the
followinginvariant:
Theno-occurrenceinvariant
Novariablebound inthe substitution occursinanyofthe right-hand sides
ofthesubstitution.
Ourcodeforsolvingequationswilldependcriticallyonthisinvariant.
(cid:3)
Exercise7.12 [ ] Using the methods in this section, derive types for each of the
expressionsin exercise 7.1, or determine that no such type exists. As in the other
examplesofthissection,assumethereisa?attachedtoeachboundvariable.

| 7.4 INFERRED:ALanguagewithTypeInference |     |     |     |     |     |     |     | 259 |
| --------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
(cid:3)
Exercise7.13 [ ]Write down a rule for doing type inference for let expressions.
Using your rule, derive types for each of the following expressions, or determine
thatnosuchtypeexists.
| 1. let | x = 4        | in (x | 3)        |        |          |     |     |     |
| ------ | ------------ | ----- | --------- | ------ | -------- | --- | --- | --- |
| 2. let | f = proc     | (z)   | z in proc | (x)    | -((f x), | 1)  |     |     |
| 3. let | p = zero?(1) |       | in if p   | then   | 88 else  | 99  |     |     |
| 4. let | p = proc     | (z)   | z in if   | p then | 88 else  | 99  |     |     |
(cid:3)
| Exercise7.14 | [   | ] Whatiswrongwiththisexpression? |     |     |     |     |     |     |
| ------------ | --- | -------------------------------- | --- | --- | --- | --- | --- | --- |
letrec
|     | ? even(odd |             | : ?) =  |        |             |         |         |     |
| --- | ---------- | ----------- | ------- | ------ | ----------- | ------- | ------- | --- |
|     | proc       | (x          | : ?)    |        |             |         |         |     |
|     | if         | zero?(x)    | then    | 1 else | (odd        | -(x,1)) |         |     |
|     | in letrec  |             |         |        |             |         |         |     |
|     | ?          | odd(x       | : bool) | =      |             |         |         |     |
|     |            | if zero?(x) | then    | 0      | else ((even | odd)    | -(x,1)) |     |
|     | in         | (odd 13)    |         |        |             |         |         |     |
(cid:3)(cid:3)
Exercise7.15 [ ] Writedownarulefordoingtypeinferenceforaletrecexpres-
sion. Yourruleshouldhandlemultipledeclarationsinaletrec. Usingyourrule,
derive types for each of the following expressions, or determine that no such type
exists:
| 1.  | letrec | ? f | (x : ?)     |      |        |      |          |     |
| --- | ------ | --- | ----------- | ---- | ------ | ---- | -------- | --- |
|     |        | =   | if zero?(x) | then | 0 else | -((f | -(x,1)), | -2) |
in f
| 2.  | letrec  | ? even | (x :          | ?)   |        |            |         |     |
| --- | ------- | ------ | ------------- | ---- | ------ | ---------- | ------- | --- |
|     |         |        | = if zero?(x) |      | then 1 | else (odd  | -(x,1)) |     |
|     |         | ? odd  | (x :          | ?)   |        |            |         |     |
|     |         |        | = if zero?(x) |      | then 0 | else (even | -(x,1)) |     |
|     | in (odd | 13)    |               |      |        |            |         |     |
|     | letrec  | ? even | (odd          | : ?) |        |            |         |     |
3.
|     |     |     | = proc (x) | if  | zero?(x) |     |     |     |
| --- | --- | --- | ---------- | --- | -------- | --- | --- | --- |
then 1
|     |           |      |             | else   | (odd         | -(x,1)) |     |     |
| --- | --------- | ---- | ----------- | ------ | ------------ | ------- | --- | --- |
|     | in letrec |      | ? odd (x    | : ?)   | =            |         |     |     |
|     |           |      | if zero?(x) |        |              |         |     |     |
|     |           |      | then        | 0      |              |         |     |     |
|     |           |      | else        | ((even | odd) -(x,1)) |         |     |     |
|     | in        | (odd | 13)         |        |              |         |     |     |
(cid:3)(cid:3)(cid:3)
Exercise7.16 [ ] Modify the grammar of INFERRED so that missing types are
simplyomitted,ratherthanmarkedwith?.

260 7 Types
7.4.1 Substitutions
Wewillbuildtheimplementationinabottom-upfashion. Wefirstconsider
substitutions.
Werepresenttypevariablesasanadditionalvariantofthetypedatatype.
We do this using the same technique that we used for lexical addresses in
| section3.7. | Weaddtothegrammartheproduction |     |     |     |     |     |     |
| ----------- | ------------------------------ | --- | --- | --- | --- | --- | --- |
Type::=%tvar-type
Number
|     |     |     | tvar-type | (serial-number) |     |     |     |
| --- | --- | --- | --------- | --------------- | --- | --- | --- |
We call these extended types type expressions. A basic operation on
type expressions is substitution of a type for a type variable, defined
| apply-one-subst. |     |     | (apply-one-subst |     |        | )           |          |
| ---------------- | --- | --- | ---------------- | --- | ------ | ----------- | -------- |
| by               |     |     |                  |     | t 0 tv | t 1 returns | the type |
obtainedbysubstitutingt foreveryoccurrenceoftvint . Thisissometimes
|     |     |     | 1   |     |     | 0   |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
=
| writtent        | [tv             | t ].      |                 |              |        |     |     |
| --------------- | --------------- | --------- | --------------- | ------------ | ------ | --- | --- |
|                 | 0               | 1         |                 |              |        |     |     |
| apply-one-subst |                 | : Type    | × Tvar          | × Type       | → Type |     |     |
| (define         | apply-one-subst |           |                 |              |        |     |     |
| (lambda         |                 | (ty0 tvar | ty1)            |              |        |     |     |
|                 | (cases          | type      | ty0             |              |        |     |     |
|                 | (int-type       |           | () (int-type))  |              |        |     |     |
|                 | (bool-type      |           | () (bool-type)) |              |        |     |     |
|                 | (proc-type      |           | (arg-type       | result-type) |        |     |     |
(proc-type
|     |            | (apply-one-subst |      | arg-type    | tvar ty1) |        |     |
| --- | ---------- | ---------------- | ---- | ----------- | --------- | ------ | --- |
|     |            | (apply-one-subst |      | result-type | tvar      | ty1))) |     |
|     | (tvar-type |                  | (sn) |             |           |        |     |
|     | (if        | (equal?          | ty0  | tvar) ty1   | ty0)))))  |        |     |
Thisproceduredealswithsubstitutingforasingletypevariable.Itdoesn’t
dealwith full-fledged substitutions like those we had in the precedingsec-
tion.
A substitution is a list of equations between type variables and types.
Equivalently, we can think of this list as a function from type variables to
types. We say a type variable is bound in the substitution if and only if it
occursontheleft-handsideofoneoftheequationsinthesubstitution.
We representa substitution as a list of pairs(type variable . type). The
basic observer for substitutions is apply-subst-to-type. This walks
throughthetypet,replacingeachtypevariablebyitsbindinginthesubstitu-
tion σ . Ifavariableisnotboundinthesubstitution,thenitisleftunchanged.
σ
| Wewritet | fortheresultingtype. |     |     |     |     |     |     |
| -------- | -------------------- | --- | --- | --- | --- | --- | --- |

| 7.4 INFERRED:ALanguagewithTypeInference |     |     |     |     |     |     |     | 261 |
| --------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
TheimplementationusestheSchemeprocedureassoctolookupthetype
variable in the substitution. assoc returns either the matching (type vari-
able,type)pairor#fifthegiventypevariableisnotthecarofanypairin
thelist. Wewrite
|                     |                     |            |                 | ×     | →    |     |     |     |
| ------------------- | ------------------- | ---------- | --------------- | ----- | ---- | --- | --- | --- |
| apply-subst-to-type |                     |            | : Type          | Subst | Type |     |     |     |
| (define             | apply-subst-to-type |            |                 |       |      |     |     |     |
| (lambda             |                     | (ty subst) |                 |       |      |     |     |     |
| (cases              |                     | type       | ty              |       |      |     |     |     |
|                     | (int-type           |            | () (int-type))  |       |      |     |     |     |
|                     | (bool-type          |            | () (bool-type)) |       |      |     |     |     |
|                     | (proc-type          |            | (t1 t2)         |       |      |     |     |     |
(proc-type
|     |            | (apply-subst-to-type |        |     | t1 subst)   |     |     |     |
| --- | ---------- | -------------------- | ------ | --- | ----------- | --- | --- | --- |
|     |            | (apply-subst-to-type |        |     | t2 subst))) |     |     |     |
|     | (tvar-type |                      | (sn)   |     |             |     |     |     |
|     | (let       | ((tmp                | (assoc | ty  | subst)))    |     |     |     |
(if tmp
|     |     | (cdr | tmp) |     |     |     |     |     |
| --- | --- | ---- | ---- | --- | --- | --- | --- | --- |
ty))))))
Theconstructorsforsubstitutionsareempty-substandextend-subst.
(empty-subst) produces a representation of the empty substitution.
| (extend-subst |     | σ   | t)       |     |              | σ   |      |              |
| ------------- | --- | --- | -------- | --- | ------------ | --- | ---- | ------------ |
|               |     |     | tv takes | the | substitution | and | adds | the equation |
tv = t to it, as we did in the preceding section. We write σ [tv = t] for the
resultingsubstitution. Thiswasatwo-step operation: firstwesubstituted t
fortvineachoftheright-handsidesoftheequationsinthesubstitution,and
=
| thenweaddedtheequationtv |     |      |       | t tothelist. | Pictorially, |           |      |     |
| ------------------------ | --- | ---- | ----- | ------------ | ------------ | --------- | ---- | --- |
|                          | ⎛   |      | ⎞     |              | ⎛            |           | ⎞    |     |
|                          |     | =    |       |              |              | =         |      |     |
|                          |     | tv 1 | t 1   |              | tv           | t         |      |     |
|                          | ⎜   |      | ⎟     |              | ⎜            |           | ⎟    |     |
|                          | ⎜   | .    | ⎟     |              | ⎜ tv         | = t [tv = | t] ⎟ |     |
|                          |     | . .  |       | = =          |              | 1 1       |      |     |
|                          | ⎜   |      | ⎟ [tv | t]           | ⎜            | .         | ⎟    |     |
|                          | ⎝   | =    | ⎠     |              | ⎝            | .         | ⎠    |     |
|                          |     | tv   | t     |              |              | .         |      |     |
|                          |     | n    | n     |              |              |           |      |     |
|                          |     |      |       |              |              | = =       |      |     |
|                          |     |      |       |              | tv           | n t n [tv | t]   |     |
Thisdefinitionhasthepropertythatforanytypet,
|     |     |     | σ       | = (cid:12) = | σ =    | (cid:12) |     |     |
| --- | --- | --- | ------- | ------------ | ------ | -------- | --- | --- |
|     |     |     | (t )[tv | t]           | t( [tv | t])      |     |     |
Theimplementationofextend-substfollowsthepictureabove. Itsub-
σ
stitutest fortv inalloftheexistingbindingsin ,andthenaddsthebind-
|     | 0   | 0   |     |     |     | 0   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
ingfort 0 .

262 7 Types
→
| empty-subst  | : ()         | Subst   |        |           |       |     |     |
| ------------ | ------------ | ------- | ------ | --------- | ----- | --- | --- |
| (define      | empty-subst  | (lambda |        | () ’()))  |       |     |     |
| extend-subst | : Subst      | ×       | Tvar × | Type →    | Subst |     |     |
| usage: tvar  | not          | already | bound  | in subst. |       |     |     |
| (define      | extend-subst |         |        |           |       |     |     |
| (lambda      | (subst       | tvar    | ty)    |           |       |     |     |
(cons
| (cons | tvar | ty) |     |     |     |     |     |
| ----- | ---- | --- | --- | --- | --- | --- | --- |
(map
(lambda (p)
|     | (let | ((oldlhs | (car | p))  |     |     |     |
| --- | ---- | -------- | ---- | ---- | --- | --- | --- |
|     |      | (oldrhs  | (cdr | p))) |     |     |     |
(cons
oldlhs
|     |     | (apply-one-subst |     | oldrhs | tvar | ty)))) |     |
| --- | --- | ---------------- | --- | ------ | ---- | ------ | --- |
subst))))
Thisimplementationpreservestheno-occurrenceinvariant,butitdoesnot
dependon,nordoesitattempttoenforceit. Thatisthejoboftheunifier,in
thenextsection.
(cid:3)(cid:3) ]Inourrepresentation,extend-substmaydoalotofworkifσ
| Exercise7.17 | [   |     |     |     |     |     |     |
| ------------ | --- | --- | --- | --- | --- | --- | --- |
islarge. Implement analternate representationinwhich extend-substisimple-
mentedas
| (define | extend-subst |      |              |     |     |     |     |
| ------- | ------------ | ---- | ------------ | --- | --- | --- | --- |
| (lambda | (subst       | tvar | ty)          |     |     |     |     |
| (cons   | (cons        | tvar | ty) subst))) |     |     |     |     |
and the extra work is shifted to apply-subst-to-type, so that the property
| t(σ[tv =t(cid:12) ])=(tσ)[tv |     | =t(cid:12) |                  |     |                 |     |                  |
| ---------------------------- | --- | ---------- | ---------------- | --- | --------------- | --- | ---------------- |
|                              |     | ] is       | still satisfied. | For | this definition |     | of extend-subst, |
istheno-occurrenceinvariantneeded?
(cid:3)(cid:3)
Exercise7.18 [ ]Modify the implementation in the preceding exercise so that
apply-subst-to-type
|     |     | computes | the | substitution | for | any type | variable at most |
| --- | --- | -------- | --- | ------------ | --- | -------- | ---------------- |
once.
7.4.2 TheUnifier
Themainprocedureoftheunifierisunifier.Theunifierperformsonestep
oftheinferenceprocedureoutlinedabove:Ittakestwotypes,t andt ,asub-
|     |     |     |     |     |     |     | 1 2 |
| --- | --- | --- | --- | --- | --- | --- | --- |
σ
stitution thatsatisfiestheno-occurrenceinvariant,andanexpressionexp.
Itreturnsthesubstitutionthatresultsfromaddingt = t to σ . Thiswillbe
1 2
|     |     | σ   |     | σ   | σ   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- |
thesmallestextensionof thatunifiest andt . Thissubstitutionwillstill
|     |     |     |     | 1   | 2   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- |

7.4 INFERRED:ALanguagewithTypeInference 263
satisfytheno-occurrenceinvariant.Ifaddingt = t yieldsaninconsistency
1 2
orviolatestheno-occurrenceinvariant,thentheunifierreportsanerror,and
blamestheexpressionexp. Thisistypicallytheexpressionthatgaveriseto
theequationt = t .
1 2
Thisisanalgorithmforwhichcasesgivesawkwardcode,soweusesim-
ple predicates and extractors on types instead. The algorithm is shown in
figure7.4,anditworksasfollows:
• First, as we did above, we apply the substitution to each of the types t
1
andt .
2
• If the resulting types are the same, we return immediately. This corre-
spondstothestepofdeletingatrivialequationabove.
• Ifty1isanunknowntype,thentheno-occurrenceinvarianttellsusthat
it is not bound in the substitution. Hence it must be unbound, so we
propose to add t = t to the substitution. But we need to perform the
1 2
occurrence check, so that the no-occurrence invariant is preserved. The
call(no-occurrence? tv t)returns#tifandonlyifthereisnooccur-
renceofthetypevariabletv int (figure7.5).
• If t is anunknown type, we do the same thing, reversingthe rolesof t
2 1
andt .
2
• Ifneithert nort isatypevariable,thenwecananalyzefurther.
1 2
Ifthey areboth proctypes, then we simplify byequating the argument
types,andthenequatingtheresulttypesintheresultingsubstitution.
Otherwise,eitheroneoft andt isintandtheotherisbool,oroneisa
1 2
proctypeandtheotherisintorbool. Inanyofthesecases,thereisno
solutiontotheequation,soanerrorisreported.
Here is another way of thinking about all this that is sometimes useful.
Thesubstitutionisastore,andanunknowntypeisareferenceintothatstore.
unifierproducesthenewstorethatisobtainedbyaddingty1 = ty2to
thestore.
Last,wemustimplementtheoccurrencecheck. Thisisastraightforward
recursiononthetype,andisshowninfigure7.5.
(cid:3)
Exercise7.19 [ ] We wrote: “If ty1 is an unknown type, then the no-occurrence
invarianttellsusthatitisnotboundinthesubstitution.” Explainindetailwhythis
isso.

264 7 Types
|         |         | × ×                  |       | ×   | →     |          |
| ------- | ------- | -------------------- | ----- | --- | ----- | -------- |
| unifier | : Type  | Type                 | Subst | Exp | Subst |          |
| (define | unifier |                      |       |     |       |          |
| (lambda | (ty1    | ty2 subst            | exp)  |     |       |          |
| (let    | ((ty1   | (apply-subst-to-type |       |     | ty1   | subst))  |
|         | (ty2    | (apply-subst-to-type |       |     | ty2   | subst))) |
(cond
|     | ((equal?                        | ty1 ty2)        |                      | subst)           |      |                |
| --- | ------------------------------- | --------------- | -------------------- | ---------------- | ---- | -------------- |
|     | ((tvar-type?                    | ty1)            |                      |                  |      |                |
|     | (if                             | (no-occurrence? |                      | ty1              | ty2) |                |
|     | (extend-subst                   |                 | subst                | ty1              | ty2) |                |
|     | (report-no-occurrence-violation |                 |                      |                  |      | ty1 ty2 exp))) |
|     | ((tvar-type?                    | ty2)            |                      |                  |      |                |
|     | (if                             | (no-occurrence? |                      | ty2              | ty1) |                |
|     | (extend-subst                   |                 | subst                | ty2              | ty1) |                |
|     | (report-no-occurrence-violation |                 |                      |                  |      | ty2 ty1 exp))) |
|     | ((and                           | (proc-type?     |                      | ty1) (proc-type? |      | ty2))          |
|     | (let                            | ((subst         | (unifier             |                  |      |                |
|     |                                 |                 | (proc-type->arg-type |                  |      | ty1)           |
|     |                                 |                 | (proc-type->arg-type |                  |      | ty2)           |
subst exp)))
|     | (let | ((subst | (unifier |                         |     |      |
| --- | ---- | ------- | -------- | ----------------------- | --- | ---- |
|     |      |         |          | (proc-type->result-type |     | ty1) |
|     |      |         |          | (proc-type->result-type |     | ty2) |
subst exp)))
subst)))
|     | (else | (report-unification-failure |     |            |     | ty1 ty2 exp)))))) |
| --- | ----- | --------------------------- | --- | ---------- | --- | ----------------- |
|     |       | Figure7.4                   |     | Theunifier |     |                   |
Exercise7.20 [ (cid:3)(cid:3) ] Modifythe unifier sothat itcallsapply-subst-to-typeonly
ontypevariables,ratherthanonitsarguments.
(cid:3)(cid:3)
Exercise7.21 [ ] We said the substitution is like a store. Implement the unifier,
usingtherepresentationofsubstitutionsfromexercise7.17,andkeepingthesubsti-
tutioninaglobalSchemevariable,aswedidinfigures4.1and4.2.
(cid:3)(cid:3)
Exercise7.22 [ ] Refine the implementation of the preceding exercise so that the
bindingofeachtypevariablecanbeobtainedinconstanttime.

7.4 INFERRED:ALanguagewithTypeInference 265
|                |                | ×           | →            |     |     |
| -------------- | -------------- | ----------- | ------------ | --- | --- |
| no-occurrence? |                | : Tvar Type | Bool         |     |     |
| (define        | no-occurrence? |             |              |     |     |
| (lambda        |                | (tvar ty)   |              |     |     |
|                | (cases         | type ty     |              |     |     |
|                | (int-type      | () #t)      |              |     |     |
|                | (bool-type     | () #t)      |              |     |     |
|                | (proc-type     | (arg-type   | result-type) |     |     |
(and
|     |            | (no-occurrence? | tvar               | arg-type)      |               |
| --- | ---------- | --------------- | ------------------ | -------------- | ------------- |
|     |            | (no-occurrence? | tvar               | result-type))) |               |
|     | (tvar-type | (serial-number) |                    | (not (equal?   | tvar ty)))))) |
|     |            | Figure7.5       | Theoccurrencecheck |                |               |
7.4.3 FindingtheTypeofanExpression
Weconvertoptionaltypestotypeswithunknownsbydefiningafreshtype
variableforeach?,usingotype->type.
→
| optype->type |             | : OptionalType        | Type  |     |     |
| ------------ | ----------- | --------------------- | ----- | --- | --- |
| (define      | otype->type |                       |       |     |     |
| (lambda      |             | (otype)               |       |     |     |
|              | (cases      | optional-type         | otype |     |     |
|              | (no-type    | () (fresh-tvar-type)) |       |     |     |
|              | (a-type     | (ty) ty))))           |       |     |     |
→
| fresh-tvar-type |                 | : () Type    |     |     |     |
| --------------- | --------------- | ------------ | --- | --- | --- |
| (define         | fresh-tvar-type |              |     |     |     |
| (let            | ((sn            | 0))          |     |     |     |
|                 | (lambda         | ()           |     |     |     |
|                 | (set!           | sn (+ sn 1)) |     |     |     |
|                 | (tvar-type      | sn))))       |     |     |     |

266 7 Types
Whenweconverttoexternalform,werepresentatypevariablebyasym-
bolcontainingitsserialnumber.
: →
| type-to-external-form | Type                  | List         |     |
| --------------------- | --------------------- | ------------ | --- |
| (define               | type-to-external-form |              |     |
| (lambda               | (ty)                  |              |     |
| (cases                | type ty               |              |     |
|                       | (int-type () ’int)    |              |     |
|                       | (bool-type () ’bool)  |              |     |
|                       | (proc-type (arg-type  | result-type) |     |
(list
|     | (type-to-external-form | arg-type) |     |
| --- | ---------------------- | --------- | --- |
’->
|     | (type-to-external-form | result-type))) |     |
| --- | ---------------------- | -------------- | --- |
(tvar-type (serial-number)
(string->symbol
(string-append
"ty"
|     | (number->string | serial-number))))))) |     |
| --- | --------------- | -------------------- | --- |
Now we canwrite type-of. It takesan expression, a type environment
mappingprogramvariablestotypeexpressions,andasubstitutionsatisfying
the no-occurrence invariant, and itreturnsa type and a new no-occurrence
substitution.
The type environment associates a type expression with each program
variable. Thesubstitutionexplainsthemeaningofeachtypevariableinthe
typeexpressions. Weusethemetaphorofasubstitutionasastore,andatype
Therefore,type-ofreturnstwovalues:
variableasreferenceintothatstore.
atypeexpression,andasubstitutioninwhichtointerpretthetypevariables
inthatexpression. Weimplementthisaswedidinexercise4.12,bydefining
a new data type that contains the two values, and using that as the return
value.
type-of
The definition of is shown in figures 7.6–7.8. For each kind of
expression, we recur on the subexpressions, passing along the solution so
far in the substitution argument. Then we generate the equations for the
current expression, according to the specification, and record these in the
substitutionbycallingunifier.
Testingtheinferencerissomewhatmore subtlethantestingourprevious
interpreters,becauseofthepossibilityofpolymorphism. Forexample,ifthe
inferencerisgivenproc (x) x,itmightgenerateanyoftheexternalforms
| (tvar1 | -> tvar1)or(tvar2 | -> tvar2)or(tvar3 | -> tvar3),and |
| ------ | ----------------- | ----------------- | ------------- |
soon. Thesemaybedifferenteverytimethroughtheinferencer,sowewon’t

7.4 INFERRED:ALanguagewithTypeInference 267
Answer = Type × Subst
(define-datatype answer answer?
(an-answer
(ty type?)
(subst substitution?)))
type-of-program : Program → Type
(define type-of-program
(lambda (pgm)
(cases program pgm
(a-program (exp1)
(cases answer (type-of exp1
(init-tenv) (empty-subst))
(an-answer (ty subst)
(apply-subst-to-type ty subst)))))))
type-of : Exp × Tenv × Subst → Answer
(define type-of
(lambda (exp tenv subst)
(cases expression exp
(const-exp (num) (an-answer (int-type) subst))
(zero?-exp e ) : t =int
1 e1
t(zero?-exp e1 ) =bool
(zero?-exp (exp1)
(cases answer (type-of exp1 tenv subst)
(an-answer (ty1 subst1)
(let ((subst2
(unifier ty1 (int-type) subst1 exp)))
(an-answer (bool-type) subst2)))))
Figure7.6 type-offorINFERRED,part1

268 7 Types
| (diff-exp |     | )       |        | =int |     |     |     |
| --------- | --- | ------- | ------ | ---- | --- | --- | --- |
|           |     | e 1 e 2 | : t e1 |      |     |     |     |
=int
t
e2
|            |        |               | t(diff-exp |                | =int           |         |         |
| ---------- | ------ | ------------- | ---------- | -------------- | -------------- | ------- | ------- |
|            |        |               |            | e1 e2          | )              |         |         |
| (diff-exp  |        | (exp1 exp2)   |            |                |                |         |         |
| (cases     | answer | (type-of      |            | exp1 tenv      | subst)         |         |         |
| (an-answer |        | (ty1          | subst1)    |                |                |         |         |
|            | (let   | ((subst1      |            |                |                |         |         |
|            |        | (unifier      |            | ty1 (int-type) |                | subst1  | exp1))) |
|            | (cases | answer        | (type-of   | exp2           | tenv           | subst1) |         |
|            |        | (an-answer    | (ty2       | subst2)        |                |         |         |
|            |        | (let ((subst2 |            |                |                |         |         |
|            |        |               | (unifier   | ty2            | (int-type)     |         |         |
|            |        |               | subst2     | exp2)))        |                |         |         |
|            |        | (an-answer    |            | (int-type)     | subst2)))))))) |         |         |
| (if-exp    |        | )             |            | =bool          |                |         |         |
|            | e      | 1 e 2 e 3     | : t e1     |                |                |         |         |
=t(if-exp
|     |     |     | t   |     |          | )   |     |
| --- | --- | --- | --- | --- | -------- | --- | --- |
|     |     |     | e2  |     | e1 e2 e3 |     |     |
t =t(if-exp
|            |        |            | e3         |                 | e1 e2 e3        | )         |         |
| ---------- | ------ | ---------- | ---------- | --------------- | --------------- | --------- | ------- |
| (if-exp    | (exp1  | exp2       | exp3)      |                 |                 |           |         |
| (cases     | answer | (type-of   |            | exp1 tenv       | subst)          |           |         |
| (an-answer |        | (ty1       | subst)     |                 |                 |           |         |
|            | (let   | ((subst    |            |                 |                 |           |         |
|            |        | (unifier   |            | ty1 (bool-type) |                 | subst     | exp1))) |
|            | (cases | answer     | (type-of   | exp2            | tenv            | subst)    |         |
|            |        | (an-answer | (ty2       | subst)          |                 |           |         |
|            |        | (cases     | answer     | (type-of        | exp3            | tenv      | subst)  |
|            |        | (an-answer |            | (ty3 subst)     |                 |           |         |
|            |        | (let       | ((subst    |                 |                 |           |         |
|            |        |            |            | (unifier        | ty2             | ty3 subst | exp)))  |
|            |        |            | (an-answer | ty2             | subst)))))))))) |           |         |
(var-exp (var)
| (an-answer |              | (apply-tenv |       | tenv var) | subst)) |     |     |
| ---------- | ------------ | ----------- | ----- | --------- | ------- | --- | --- |
| (let-exp   | (var         | exp1        | body) |           |         |     |     |
| (cases     | answer       | (type-of    |       | exp1 tenv | subst)  |     |     |
| (an-answer |              | (exp1-type  |       | subst)    |         |     |     |
|            | (type-of     | body        |       |           |         |     |     |
|            | (extend-tenv |             | var   | exp1-type | tenv)   |     |     |
subst))))
|     | Figure7.7 | type-offorINFERRED,part2 |     |     |     |     |     |
| --- | --------- | ------------------------ | --- | --- | --- | --- | --- |

| 7.4 INFERRED:ALanguagewithTypeInference |           |        |            |              |              |      |              |     | 269   |
| --------------------------------------- | --------- | ------ | ---------- | ------------ | ------------ | ---- | ------------ | --- | ----- |
|                                         | (proc-exp |        | var        | body)        | : t          |      | =(t          | →   | t )   |
|                                         |           |        |            |              | (proc-exp    |      | var body)    | var | body  |
|                                         | (proc-exp |        | (var       | otype        | body)        |      |              |     |       |
|                                         | (let      |        | ((var-type | (otype->type |              |      | otype)))     |     |       |
|                                         |           | (cases | answer     | (type-of     |              | body |              |     |       |
|                                         |           |        |            |              | (extend-tenv |      | var var-type |     | tenv) |
subst)
|     |     | (an-answer |     | (body-type |     | subst) |     |     |     |
| --- | --- | ---------- | --- | ---------- | --- | ------ | --- | --- | --- |
(an-answer
|     |     |     | (proc-type |     | var-type |     | body-type) |     |     |
| --- | --- | --- | ---------- | --- | -------- | --- | ---------- | --- | --- |
subst)))))
|     |           |            |               |             |                     | =(t    | →           |        |        |
| --- | --------- | ---------- | ------------- | ----------- | ------------------- | ------ | ----------- | ------ | ------ |
|     | (call-exp |            | rator         | rand)       | : t                 |        | t(call-exp  |        | rand)) |
|     |           |            |               |             |                     | rator  | rand        |        | rator  |
|     | (call-exp |            | (rator        | rand)       |                     |        |             |        |        |
|     | (let      |            | ((result-type |             | (fresh-tvar-type))) |        |             |        |        |
|     |           | (cases     | answer        | (type-of    |                     | rator  | tenv subst) |        |        |
|     |           | (an-answer |               | (rator-type |                     | subst) |             |        |        |
|     |           |            | (cases        | answer      | (type-of            |        | rand tenv   | subst) |        |
|     |           |            | (an-answer    |             | (rand-type          |        | subst)      |        |        |
|     |           |            | (let          | ((subst     |                     |        |             |        |        |
(unifier
rator-type
(proc-type
|     |     |     |     |     | rand-type |     | result-type) |     |     |
| --- | --- | --- | --- | --- | --------- | --- | ------------ | --- | --- |
subst
exp)))
|     |     |     |     | (an-answer |     | result-type | subst)))))))) |     |     |
| --- | --- | --- | --- | ---------- | --- | ----------- | ------------- | --- | --- |
type-offorINFERRED,part3
Figure7.8
beabletoanticipatethem whenwe write ourtestitems. Sowhenwe com-
pare the produced type to the correct type, we’ll fail. We need to accept
|        |                  |     |        |     |        | (tvar3 | -> tvar4) |     | (int -> |
| ------ | ---------------- | --- | ------ | --- | ------ | ------ | --------- | --- | ------- |
| all of | the alternatives |     | above, | but | reject |        |           | or  |         |
tvar17).
To compare two types in externalform, we standardizethe namesof the
unknown types, by walking through each external form, renumbering all
thetypevariablessothattheyarenumberedstartingwithty1. Wecanthen
comparetherenumberedtypeswithequal?(figures7.10–7.11).
Tosystematicallyrenameeachunknowntype,weconstructasubstitution
with canonical-subst. This is a straightforward recursion, with table

270 7 Types
letrect proc−result p(var : t var )=e proc-body ine letrec-body :
t =t →t
p var eproc-body
t =t
eletrec-body letrectproc−result p(var : tvar)=eproc-body ineletrec-body
(letrec-exp (p-result-otype p-name b-var b-var-otype
p-body letrec-body)
(let ((p-result-type (otype->type p-result-otype))
(p-var-type (otype->type b-var-otype)))
(let ((tenv-for-letrec-body
(extend-tenv p-name
(proc-type p-var-type p-result-type)
tenv)))
(cases answer (type-of p-body
(extend-tenv b-var p-var-type
tenv-for-letrec-body)
subst)
(an-answer (p-body-type subst)
(let ((subst
(unifier p-body-type p-result-type
subst p-body)))
(type-of letrec-body
tenv-for-letrec-body
subst))))))))))
Figure7.9 type-offorINFERRED,part4
playingtheroleofanaccumulator. Thelengthoftabletellsushowmany
distinctunknown typeswehavefound,sowecanuseitslengthtogivethe
numberofthe“next”tysymbol. Thisissimilartothewayweusedlength
infigure4.1.
(cid:3)(cid:3)
Exercise7.23 [ ] Extendtheinferencertohandlepairtypes,asinexercise7.8.
(cid:3)(cid:3)
Exercise7.24 [ ]Extendtheinferencertohandlemultipleletdeclarations,multi-
argumentprocedures,andmultipleletrecdeclarations.

7.4 INFERRED:ALanguagewithTypeInference 271
| TvarTypeSym |     | = a | symbol | ending with | a digit |     |
| ----------- | --- | --- | ------ | ----------- | ------- | --- |
Listof(Pair(TvarTypeSym,TvarTypeSym))
A-list =
| equal-up-to-gensyms? |                      |        | :      | S-exp × S-exp | → Bool |     |
| -------------------- | -------------------- | ------ | ------ | ------------- | ------ | --- |
| (define              | equal-up-to-gensyms? |        |        |               |        |     |
| (lambda              |                      | (sexp1 | sexp2) |               |        |     |
(equal?
|                 | (apply-subst-to-sexp |        |        | (canonical-subst |       | sexp1) sexp1)    |
| --------------- | -------------------- | ------ | ------ | ---------------- | ----- | ---------------- |
|                 | (apply-subst-to-sexp |        |        | (canonical-subst |       | sexp2) sexp2)))) |
| canonical-subst |                      | :      | S-exp  | → A-list         |       |                  |
| (define         | canonical-subst      |        |        |                  |       |                  |
| (lambda         |                      | (sexp) |        |                  |       |                  |
|                 |                      |        | ×      | →                |       |                  |
|                 | loop :               | S-exp  | A-list | A-list           |       |                  |
|                 | (let loop            | ((sexp |        | sexp) (table     | ’())) |                  |
(cond
|     | ((null?          |     | sexp) | table) |     |     |
| --- | ---------------- | --- | ----- | ------ | --- | --- |
|     | ((tvar-type-sym? |     |       | sexp)  |     |     |
(cond
|     |     | ((assq | sexp | table) | table) |     |
| --- | --- | ------ | ---- | ------ | ------ | --- |
(else
(cons
|     |     |     | (cons | sexp (ctr->ty | (length | table))) |
| --- | --- | --- | ----- | ------------- | ------- | -------- |
table))))
|     | ((pair? |            | sexp) |                            |     |     |
| --- | ------- | ---------- | ----- | -------------------------- | --- | --- |
|     | (loop   |            | (cdr  | sexp)                      |     |     |
|     |         | (loop      | (car  | sexp) table)))             |     |     |
|     | (else   | table))))) |       |                            |     |     |
|     |         | Figure7.10 |       | equal-up-to-gensyms?,part1 |     |     |
(cid:3)(cid:3)
Exercise7.25 [ ]Extendtheinferencertohandlelisttypes,asinexercise7.9.Mod-
ifythelanguagetousetheproduction
Expression::=emptylist
insteadof
Expression::=emptylist_Type
Asahint,considercreatingatypevariableinplaceofthemissing_t.

272 7 Types
→
| tvar-type-sym? |                | : Sym | Bool |          |               |     |            |
| -------------- | -------------- | ----- | ---- | -------- | ------------- | --- | ---------- |
| (define        | tvar-type-sym? |       |      |          |               |     |            |
| (lambda        | (sym)          |       |      |          |               |     |            |
|                | (and (symbol?  |       | sym) |          |               |     |            |
|                | (char-numeric? |       | (car | (reverse | (symbol->list |     | sym))))))) |
→
| symbol->list        | :                   | Sym   | List            |         |         |     |     |
| ------------------- | ------------------- | ----- | --------------- | ------- | ------- | --- | --- |
| (define             | symbol->list        |       |                 |         |         |     |     |
| (lambda             | (x)                 |       |                 |         |         |     |     |
|                     | (string->list       |       | (symbol->string |         | x))))   |     |     |
| apply-subst-to-sexp |                     | :     | A-list          | × S-exp | → S-exp |     |     |
| (define             | apply-subst-to-sexp |       |                 |         |         |     |     |
| (lambda             | (subst              | sexp) |                 |         |         |     |     |
(cond
|     | ((null?          | sexp) | sexp) |          |     |     |     |
| --- | ---------------- | ----- | ----- | -------- | --- | --- | --- |
|     | ((tvar-type-sym? |       |       | sexp)    |     |     |     |
|     | (cdr (assq       |       | sexp  | subst))) |     |     |     |
|     | ((pair?          | sexp) |       |          |     |     |     |
(cons
|         | (apply-subst-to-sexp |     |     | subst | (car | sexp))   |     |
| ------- | -------------------- | --- | --- | ----- | ---- | -------- | --- |
|         | (apply-subst-to-sexp |     |     | subst | (cdr | sexp)))) |     |
|         | (else sexp))))       |     |     |       |      |          |     |
|         | : →                  |     |     |       |      |          |     |
| ctr->ty | N                    | Sym |     |       |      |          |     |
| (define | ctr->ty              |     |     |       |      |          |     |
| (lambda | (n)                  |     |     |       |      |          |     |
(string->symbol
|     | (string-append |            | "tvar" | (number->string            |     | n))))) |     |
| --- | -------------- | ---------- | ------ | -------------------------- | --- | ------ | --- |
|     |                | Figure7.11 |        | equal-up-to-gensyms?,part2 |     |        |     |
(cid:3)(cid:3)
Exercise7.26 [ ] Extend the inferencer to handle EXPLICIT-REFS, as in exer-
cise7.10.
(cid:3)(cid:3)
Exercise7.27 [ ]Rewritetheinferencersothatitworksintwophases. Inthefirst
phaseitshouldgenerateasetofequations,andinthesecondphase,itshouldrepeat-
edlycallunifytosolvethem.

7.4 INFERRED:ALanguagewithTypeInference 273
(cid:3)(cid:3)
Exercise7.28 [ ] Our inferencer is very useful, but it is not powerful enough to
allowtheprogrammertodefineproceduresthatarepolymorphic,likethepolymor-
phicprimitivespairorcons,whichcanbeusedatmanytypes. Forexample,our
inferencerwouldrejecttheprogram
| let f = proc | (x : ?) | x   |
| ------------ | ------- | --- |
in if (f zero?(0))
then (f 11)
else (f 22)
eventhoughitsexecutionissafe,becausefisusedbothattype(bool→ bool)and
attype(int→ int). Sincetheinferencerofthissectionisallowedtofindatmost
onetypeforf,itwillrejectthisprogram.
Foramorerealisticexample,onewouldliketowriteprogramslike
let
| ? map (f : | ?) = |     |
| ---------- | ---- | --- |
letrec
| ? foo | (x : ?) = | if null?(x)           |
| ----- | --------- | --------------------- |
|       |           | then emptylist        |
|       |           | else cons((f car(x)), |
((foo f) cdr(x)))
in foo
in letrec
| ? even | (y : ?) = | if zero?(y)           |
| ------ | --------- | --------------------- |
|        |           | then zero?(0)         |
|        |           | else if zero?(-(y,1)) |
then zero?(1)
else (even -(y,2))
| in pair(((map | proc(x | : int)-(x,1)) |
| ------------- | ------ | ------------- |
cons(3,cons(5,emptylist))),
|     | ((map even) |     |
| --- | ----------- | --- |
cons(3,cons(5,emptylist))))
This expressionuses maptwice, once producinga listof ints and once producing
bools.
a list of Therefore it needs two different types for the two uses. Since the
typeformap,
inferencerofthis sectionwillfind atmostone itwilldetectthe clash
betweenintandboolandrejecttheprogram.
One way to avoid this problem is to allow polymorphic values to be introduced
onlybylet,andthentotreat(let-exp )differentlyfrom(call-exp
var e 1 e 2
| (proc-exp var e 2 ) | e 1 )fortype-checkingpurposes. |     |
| ------------------- | ------------------------------ | --- |
Addpolymorphicbindingstotheinferencerbytreating(let-exp )like
var e 1 e 2
theexpressionobtainedbysubstitutinge 1 foreachfreeoccurrenceofvarine 2 .Then,
fromthepointofviewoftheinferencer,therearemanydifferentcopiesofe 1 inthe
bodyofthe let, sothey can have differenttypes, and the programsabove willbe
accepted.

274 7 Types
(cid:3)(cid:3)(cid:3)
Exercise7.29 [ ]Thetypeinferencealgorithmsuggestedintheprecedingexercise
willanalyzee manytimes,onceforeachofitsoccurrencesine .ImplementMilner’s
1 2
AlgorithmW,whichanalyzese onlyonce.
1
(cid:3)(cid:3)(cid:3)
Exercise7.30 [ ]The interaction between polymorphism and effects is subtle.
Consideraprogramstarting
let p = newref(proc (x : ?) x)
in ...
1. Finishthisprogramtoproduceaprogramthatpassesthepolymorphicinferencer,
butwhoseevaluationisnotsafeaccordingtothedefinitionatthebeginningofthe
chapter.
2. Avoidthisdifficultybyrestrictingtheright-handsideofalettohavenoeffect
onthestore.Thisiscalledthevaluerestriction.

8
Modules
Thelanguagefeatureswehaveintroducedsofarareverypowerfulforbuild-
ingsystemsofafewhundredlinesofcode. Ifwearetobuildlargersystems,
withthousandsoflinesofcode,wewillneedsomemoreingredients.
1. We will need a good way to separate the system into relatively self-
containedparts,andtodocumentthedependenciesbetweenthoseparts.
2. We will need a better way to control the scope and binding of names.
Lexicalscopingisapowerfultoolfornamecontrol,butitisnotsufficient
whenprogramsmaybelargeorsplitupovermultiplesources.
3. We will need a way to enforce abstraction boundaries. In chapter 2, we
introducedtheideaofanabstractdatatype.Insidetheimplementationof
thetype,wecanmanipulatethevaluesarbitrarily,butoutsidetheimple-
mentation,thevaluesofthetypearetobecreatedandmanipulatedonly
bytheproceduresintheinterfaceofthattype. Wecallthisanabstraction
boundary. Ifaprogramrespectsthisboundary,wecanchangetheimple-
mentation of the data type. If, however, some piece of code breaks the
abstractionbyrelyingon the detailsof the implementation, thenwe can
nolongerchangetheimplementationfreelywithoutbreakingothercode.
4. Last,weneedawaytocombine thesepartsflexibly, sothatasingle part
maybereusedindifferentcontexts.
Inthischapter,weintroducemodulesasawayofsatisfyingtheseneeds. In
particular, we show how we can use the type system to create and enforce
abstractionboundaries.
A program in our module language consists of a sequence of module def-
initions followed by anexpression to be evaluated. Eachmodule definition
bindsanametoamodule.Acreatedmoduleiseitherasimplemodule,whichis
asetofbindings,muchlikeanenvironment,oramoduleprocedurethattakes
amoduleandproducesanothermodule.

276 8 Modules
Each module will have an interface. A module that is a set of bindings
will have a simple interface, which lists the bindings offeredby the module,
andtheirtypes. Amoduleprocedurewillhaveaninterfacethatspecifiesthe
interfacesof the argumentand resultmodules of the procedure, much as a
procedurehasatypethatspecifiesthetypesofitsargumentandresult.
Theseinterfaces,liketypes,determinethewaysinwhichmodulescanbe
combined. We thereforeemphasizethe types of our examples, since evalu-
ationoftheseprogramsisstraightforward. Aswehaveseenbefore,under-
standing the scoping and binding rules of the language will be the key to
bothanalyzingandevaluatingprogramsinthelanguage.
8.1 The SimpleModule System
Our first language, SIMPLE-MODULES,has only simple modules. It does
not have module procedures, and it creates only very simple abstraction
boundaries. This module system is similar to that used in several popular
languages.
8.1.1 Examples
Imagineasoftwareprojectinvolvingthreedevelopers:Alice,Bob,andChar-
lie. Alice,Bob,andCharliearedevelopinglargelyindependentpiecesofthe
project. Thesedevelopersaregeographicallydispersed,perhapsindifferent
timezones. Eachpieceoftheprojectistoimplementaninterface,likethose
in section 2.1, but the implementation of that interface may involve a large
numberofadditionalprocedures.Furthermore,eachofthedevelopersneeds
to make sure that there are no name conflicts that would interfere with the
otherportionsoftheprojectwhenthepiecesareintegrated.
Toaccomplishthisgoal,eachofthedevelopersneedstopublishaninter-
face,listingthenamesforeachoftheirproceduresthattheyexpectothersto
use. It will be the job of the module system to ensure that these names are
public,butanyothernamestheyuseareprivateandwillnotbeoverridden
byanyotherpieceofcodeintheproject.
We could use the scoping techniques of chapter 3, but these do not scale
tolargerprojects. Instead,wewilluseamodulesystem. Eachofourdevel-
opers will produce a module consisting of a public interface and a private
implementation. Each developer can see the interface and implementation
ofhis or her own module, butAlicecansee only the interfacesof the other
modules. Nothingshecandocaninterferewiththeimplementationsofthe
other modules, nor can their module implementations interfere with hers.
(Seefigure8.1.)

8.1 TheSimpleModuleSystem 277
Charlie’s module
Charlie’s interface
quux : (int −> int)
baz : (bool −> int)
Bob’s module
Bob’s interface Charlie’s implementation
foo : (int −> int)
baz : (bool −> int)
Bob’s implementation
Alice’s module
Alice’s interface
foo : (int −> int)
bar : (int −> bool)
Alice’s implementation
foo−helper = ...
foo = proc (x : int) ...
bar = proc (x : int) ...
Figure8.1 Alice’sviewofthethreemodulesintheproject
HereisashortexampleinSIMPLE-MODULES.
Example8.1
module m1
interface
[a : int
b : int
c : int]
body
[a = 33
x = -(a,1) % = 32
b = -(a,x) % = 1
c = -(x,b)] % = 31
let a = 10
in -(-(from m1 take a,
from m1 take b),
a)
hastypeintandvalue((33 − 1) − 10) = 22.

278 8 Modules
This programbegins with the definition of a module named m1. Like all
modules, it has an interface and a body. The body implements the interface.
Theinterfacedeclaresthevariablesa,b,andc. Thebodydefinesbindingsfor
a,x,b,andc.
When we evaluate the program, the expressions in m1’s body are eval-
uated. The appropriate values are bound to the variables from m1 take
a, from m1 take b, and from m1 take c, which are in scope after the
moduledefinition. from m1 take xisnotinscopeafterthemoduledefi-
nition,sinceithasnotbeendeclaredintheinterface.
Thesenewvariablesarecalledqualified variablestodistinguishthemfrom
ourprevioussimplevariables. Inconventionallanguages,qualifiedvariables
mightbewrittenm1.aorm1:aorm1::a. Thenotation m1.aisoftenused
for something different in object-oriented languages, which we study in
chapter9.
Wesaythattheinterfaceoffers(oradvertisesor promises)threeintegerval-
ues,andthatthebodysupplies(orprovidesorexports)thesevalues.Amodule
bodysatisfiesaninterfacewhenitsuppliesavalueoftheadvertisedtypefor
eachofthevariablesthatarenamedintheinterface.
In the body, definitions have let∗ scoping, so that a is in scope in the
definitionsofx,b,andc. Someofthescopesarepicturedinfigure8.2.
Inthisexample,theexpression,startingwithlet a = 10,istheprogram
body.Itsvaluewillbecomethevalueoftheprogram.
Each module establishes an abstraction boundary between the module
bodyand the restof the program. The expressionsin the module bodyare
insidetheabstractionboundary,andeverythingelseisoutsidetheabstraction
boundary. A module body may supply bindings for names that are not in
the interface, but those bindings are not visible in the program body or in
othermodules, assuggested infigure8.1. Inour example,from m1 take
x is not in scope. Had we written -(from m1 take a, from m1 take
x),theresultingprogramwouldhavebeenill-typed.
Example8.2Theprogram
module m1
interface
[u : bool]
body
[u = 33]
44
isnotwell-typed. Thebodyofthemodulemustassociateeachnameinthe
interface with a value of the appropriate type, even if those values are not
usedelsewhereintheprogram.

8.1 TheSimpleModuleSystem 279
Figure8.2 Someofthescopesforasimplemodule
Example8.3 The module body must supply bindings for all the declara-
tionsintheinterface.Forexample,
module m1
interface
[u : int
v : int]
body
[u = 33]
44
isnotwell-typed,becausethebodyof m1doesnotprovideallofthevalues
thatitsinterfaceadvertises.

280 8 Modules
Example 8.4 To keep the implementation simple, our language requires
thatthemodulebodyproducethevaluesinthesameorderastheinterface.
Hence
module m1
interface
[u : int
v : int]
body
[v = 33
u = 44]
from m1 take u
isnotwell-typed. Thiscanbefixed(exercises8.8,8.17).
Example8.5Inourlanguage,moduleshavelet∗
scoping(exercise3.17).
Forexample,
module m1
interface
[u : int]
body
[u = 44]
module m2
interface
[v : int]
body
[v = -(from m1 take u,11)]
-(from m1 take u, from m2 take v)
hastypeint. Butifwereversetheorderofthedefinitions,weget
module m2
interface
[v : int]
body
[v = -(from m1 take u,11)]
module m1
interface
[u : int]
body
[u = 44]
-(from m1 take u, from m2 take v)
whichisnotwell-typed,sincefrom m1 take uisnotinscopewhereitis
usedinthebodyofm2.

8.1 TheSimpleModuleSystem 281
8.1.2 ImplementingtheSimpleModuleSystem
Syntax
AprograminSIMPLE-MODULESconsists of a sequence of module defini-
tions,followedbyanexpression.
program::={ModuleDefn}∗ Expression
a-program (m-defs body)
Amoduledefinitionconsistsofitsname,itsinterface,anditsbody.
ModuleDefn::=module Identifier interface Iface body ModuleBody
a-module-definition (m-name expected-iface m-body)
Aninterfacefor a simple module consists of anarbitrarynumber of dec-
larations. Eachdeclarationdeclaresaprogramvariableanditstype. Wecall
thesevaluedeclarations,sincethevariablebeingdeclaredwilldenoteavalue.
Inlatersections,weintroduceotherkindsofinterfacesanddeclarations.
Iface::=[ {Decl}∗ ]
simple-iface (decls)
Decl::=Identifier : Type
val-decl (var-name ty)
Amodulebodyconsistsofanarbitrarynumberofdefinitions. Eachdefi-
nitionassociatesavariablewiththevalueofanexpression.
ModuleBody::=[ {Defn}∗ ]
defns-module-body (defns)
Defn ::=Identifier = Expression
val-defn (var-name exp)
Our expressions arethose of CHECKED(section 7.3), but we modify the
grammartoaddanewkindofexpressionforareferencetoaqualifiedvari-
able.
Expression::=from Identifier take Identifier
qualified-var-exp (m-name var-name)

282 8 Modules
TheInterpreter
Evaluation of a module body will produce a module. In our simple mod-
ule language, a module will be an environment consisting of all the bind-
ings exported by the module. We represent these with the data type
typed-module.
| (define-datatype | typed-module | typed-module? |     |
| ---------------- | ------------ | ------------- | --- |
(simple-module
| (bindings | environment?))) |     |     |
| --------- | --------------- | --- | --- |
Webindmodulenamesintheenvironment,usinganewkindofbinding:
| (define-datatype | environment | environment? |     |
| ---------------- | ----------- | ------------ | --- |
(empty-env)
| (extend-env     | ...asbefore...) |     |     |
| --------------- | --------------- | --- | --- |
| (extend-env-rec | ...asbefore...) |     |     |
(extend-env-with-module
| (m-name symbol?)      |                 |     |     |
| --------------------- | --------------- | --- | --- |
| (m-val typed-module?) |                 |     |     |
| (saved-env            | environment?))) |     |     |
Forexample,ifourprogramis
| module m1 |     |     |     |
| --------- | --- | --- | --- |
interface
| [a : int |     |     |     |
| -------- | --- | --- | --- |
| b : int  |     |     |     |
| c : int] |     |     |     |
body
| [a = 33   |     |     |     |
| --------- | --- | --- | --- |
| b = 44    |     |     |     |
| c = 55]   |     |     |     |
| module m2 |     |     |     |
interface
| [a : int |     |     |     |
| -------- | --- | --- | --- |
| b : int] |     |     |     |
body
| [a = 66        |         |                 |     |
| -------------- | ------- | --------------- | --- |
| b = 77]        |         |                 |     |
| let z = 99     |         |                 |     |
| in -(z, -(from | m1 take | a, from m2 take | a)) |
thentheenvironmentafterthedeclarationofzis

8.1 TheSimpleModuleSystem 283
#(struct:extend-env
| z #(struct:num-val | 99) |     |
| ------------------ | --- | --- |
#(struct:extend-env-with-module
m2 #(struct:simple-module
#(struct:extend-env
|     | a #(struct:num-val 66) |     |
| --- | ---------------------- | --- |
#(struct:extend-env
|     | b #(struct:num-val | 77) |
| --- | ------------------ | --- |
#(struct:empty-env))))
#(struct:extend-env-with-module
| m1 #(struct:simple-module |     |     |
| ------------------------- | --- | --- |
#(struct:extend-env
|     | a #(struct:num-val | 33) |
| --- | ------------------ | --- |
#(struct:extend-env
|     | b #(struct:num-val | 44) |
| --- | ------------------ | --- |
#(struct:extend-env
|     | c #(struct:num-val | 55) |
| --- | ------------------ | --- |
#(struct:empty-env)))))
#(struct:empty-env))))
In this environment, both m1 and m2 are bound to simple modules, which
containasmallenvironment.
Toevaluateareferencetoaqualifiedvariablefrom m take var,weuse
lookup-qualified-var-in-env.Thisfirstlooksupthemoduleminthe
currentenvironment,andthenlooksupvarintheresultingenvironment.
× × →
| lookup-qualified-var-in-env | : Sym Sym | Env ExpVal |
| --------------------------- | --------- | ---------- |
(define lookup-qualified-var-in-env
| (lambda (m-name     | var-name env)              |               |
| ------------------- | -------------------------- | ------------- |
| (let ((m-val        | (lookup-module-name-in-env | m-name env))) |
| (cases typed-module | m-val                      |               |
(simple-module (bindings)
(apply-env bindings var-name))))))
Toevaluateaprogram,weevaluateitsbodyinaninitialenvironmentbuilt
by adding all the module definitions to the environment. The procedure
add-module-defns-to-env loops through the module definitions. For
eachmodule definition, the body is evaluated, and the resulting module is
| addedtotheenvironment. | Seefigure8.3. |     |
| ---------------------- | ------------- | --- |
Last,toevaluateamodulebody,webuildanenvironment,evaluatingeach
appropriateenvironmenttogetlet∗
| expressioninthe |     | scoping. Theproce- |
| --------------- | --- | ------------------ |
dure defns-to-env produces an environment containing only the bind-
ingsproducedbythedefinitionsdefns(figure8.4).

284 8 Modules
→
| value-of-program        |                          | : Program |                | ExpVal  |                   |     |
| ----------------------- | ------------------------ | --------- | -------------- | ------- | ----------------- | --- |
| (define                 | value-of-program         |           |                |         |                   |     |
| (lambda                 | (pgm)                    |           |                |         |                   |     |
| (cases                  |                          | program   | pgm            |         |                   |     |
|                         | (a-program               |           | (m-defns       | body)   |                   |     |
|                         | (value-of                |           | body           |         |                   |     |
|                         | (add-module-defns-to-env |           |                | m-defns | (empty-env))))))) |     |
|                         |                          |           |                | × →     |                   |     |
| add-module-defns-to-env |                          |           | : Listof(Defn) | Env     | Env               |     |
| (define                 | add-module-defns-to-env  |           |                |         |                   |     |
| (lambda                 | (defns                   |           | env)           |         |                   |     |
| (if                     | (null?                   | defns)    |                |         |                   |     |
env
|     | (cases               | module-definition |     | (car defns)   |         |     |
| --- | -------------------- | ----------------- | --- | ------------- | ------- | --- |
|     | (a-module-definition |                   |     | (m-name iface | m-body) |     |
(add-module-defns-to-env
|     |     | (cdr | defns) |     |     |     |
| --- | --- | ---- | ------ | --- | --- | --- |
(extend-env-with-module
m-name
|     |     | (value-of-module-body |     | m-body | env) |     |
| --- | --- | --------------------- | --- | ------ | ---- | --- |
env)))))))
|     | Figure8.3 |     | InterpreterforSIMPLE-MODULES,part1 |     |     |     |
| --- | --------- | --- | ---------------------------------- | --- | --- | --- |
TheChecker
The job of the checker is to make sure that each module body satisfies its
interface,andthateachvariableisusedconsistentlywithitstype.
The scoping rules of our language are fairly simple: Modules follow
let∗
| scoping, | putting |     | into scope | qualified variables | for each of | the bind- |
| -------- | ------- | --- | ---------- | ------------------- | ----------- | --------- |
ingsexportedbythemodule. Theinterfacetellsusthetypeofeachqualified
variable. Declarationsanddefinitionsbothfollowlet∗ scopingaswell(see
figure8.2).
| Aswedidwiththecheckerinchapter7, |     |     |     | weusethe | typeenvironmentto |     |
| -------------------------------- | --- | --- | --- | -------- | ----------------- | --- |
keep track of information about each name that is in scope. Since we now
| havemodulenames,webindmodulenamesinthetypeenvironment. |     |     |     |     |     | Each |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | ---- |
modulenamewillbeboundtoitsinterface,whichplaystheroleofatype.

8.1 TheSimpleModuleSystem 285
|                      |                      |      |            | ×   | →               |     |
| -------------------- | -------------------- | ---- | ---------- | --- | --------------- | --- |
| value-of-module-body |                      | :    | ModuleBody |     | Env TypedModule |     |
| (define              | value-of-module-body |      |            |     |                 |     |
| (lambda              | (m-body              | env) |            |     |                 |     |
| (cases               | module-body          |      | m-body     |     |                 |     |
| (defns-module-body   |                      |      | (defns)    |     |                 |     |
(simple-module
|              | (defns-to-env  |        | defns | env)))))) |     |     |
| ------------ | -------------- | ------ | ----- | --------- | --- | --- |
|              |                |        | ×     | →         |     |     |
| defns-to-env | : Listof(Defn) |        | Env   | Env       |     |     |
| (define      | defns-to-env   |        |       |           |     |     |
| (lambda      | (defns         | env)   |       |           |     |     |
| (if          | (null?         | defns) |       |           |     |     |
(empty-env)
| (cases | definition |             | (car        | defns) |        |            |
| ------ | ---------- | ----------- | ----------- | ------ | ------ | ---------- |
|        | (val-defn  | (var        | exp)        |        |        |            |
|        | (let       | ((val       | (value-of   | exp    | env))) |            |
|        | (let       | ((new-env   | (extend-env |        | var    | val env))) |
|        |            | (extend-env | var         | val    |        |            |
(defns-to-env
|                  |           | (cdr                               | defns) | new-env))))))))) |                   |     |
| ---------------- | --------- | ---------------------------------- | ------ | ---------------- | ----------------- | --- |
|                  | Figure8.4 | InterpreterforSIMPLE-MODULES,part2 |        |                  |                   |     |
| (define-datatype |           | type-environment                   |        |                  | type-environment? |     |
(empty-tenv)
| (extend-tenv |     | ...asbefore...) |     |     |     |     |
| ------------ | --- | --------------- | --- | --- | --- | --- |
(extend-tenv-with-module
| (name       | symbol?) |                      |     |     |     |     |
| ----------- | -------- | -------------------- | --- | --- | --- | --- |
| (interface  |          | interface?)          |     |     |     |     |
| (saved-tenv |          | type-environment?))) |     |     |     |     |
Wefindthetypeofaqualifiedvariablefrom m take var byfirstlook-
ingupminthetypeenvironment,andthenlookingupthetypeofvarinthe
resultinginterface.
| lookup-qualified-var-in-tenv |                                |                             | : Sym   | × Sym | × Tenv   | → Type         |
| ---------------------------- | ------------------------------ | --------------------------- | ------- | ----- | -------- | -------------- |
| (define                      | lookup-qualified-var-in-tenv   |                             |         |       |          |                |
| (lambda                      | (m-name                        | var-name                    | tenv)   |       |          |                |
| (let                         | ((iface                        | (lookup-module-name-in-tenv |         |       |          | tenv m-name))) |
| (cases                       | interface                      |                             | iface   |       |          |                |
|                              | (simple-iface                  |                             | (decls) |       |          |                |
|                              | (lookup-variable-name-in-decls |                             |         |       | var-name | decls))))))    |

286 8 Modules
→
| type-of-program |                 |                           | : Program     | Type |       |              |     |     |     |
| --------------- | --------------- | ------------------------- | ------------- | ---- | ----- | ------------ | --- | --- | --- |
| (define         | type-of-program |                           |               |      |       |              |     |     |     |
| (lambda         |                 | (pgm)                     |               |      |       |              |     |     |     |
|                 | (cases          | program                   | pgm           |      |       |              |     |     |     |
|                 | (a-program      |                           | (module-defns |      | body) |              |     |     |     |
|                 | (type-of        |                           | body          |      |       |              |     |     |     |
|                 |                 | (add-module-defns-to-tenv |               |      |       | module-defns |     |     |     |
(empty-tenv)))))))
| add-module-defns-to-tenv |                          |        |        | : Listof(ModuleDefn) |     | ×   | Tenv → Tenv |     |     |
| ------------------------ | ------------------------ | ------ | ------ | -------------------- | --- | --- | ----------- | --- | --- |
| (define                  | add-module-defns-to-tenv |        |        |                      |     |     |             |     |     |
| (lambda                  |                          | (defns | tenv)  |                      |     |     |             |     |     |
|                          | (if (null?               |        | defns) |                      |     |     |             |     |     |
tenv
|     | (cases               | module-definition |                |              | (car          | defns)         |        |         |     |
| --- | -------------------- | ----------------- | -------------- | ------------ | ------------- | -------------- | ------ | ------- | --- |
|     | (a-module-definition |                   |                |              | (m-name       | expected-iface |        | m-body) |     |
|     |                      | (let              | ((actual-iface |              | (interface-of |                | m-body | tenv))) |     |
|     |                      | (if               | (<:-iface      | actual-iface |               | expected-iface |        | tenv)   |     |
|     |                      | (let              | ((new-tenv     |              |               |                |        |         |     |
(extend-tenv-with-module
m-name
expected-iface
tenv)))
(add-module-defns-to-tenv
|     |     |     | (cdr | defns) | new-tenv)) |     |     |     |     |
| --- | --- | --- | ---- | ------ | ---------- | --- | --- | --- | --- |
(report-module-doesnt-satisfy-iface
|     |     |           | m-name                         | expected-iface |     | actual-iface)))))))) |     |     |     |
| --- | --- | --------- | ------------------------------ | -------------- | --- | -------------------- | --- | --- | --- |
|     |     | Figure8.5 | CheckerforSIMPLE-MODULES,part1 |                |     |                      |     |     |     |
Just as in chapter 7, the process of typechecking a program mimics the
evaluation of the program, except that instead of keeping track of val-
value-of-program,
| ues, we          | keep | track | of types. | Instead |                             | of  |     | we  | have |
| ---------------- | ---- | ----- | --------- | ------- | --------------------------- | --- | --- | --- | ---- |
| type-of-program, |      |       | and       | instead | of add-module-defns-to-env, |     |     |     | we   |
haveadd-module-defns-to-tenv.Theprocedureadd-module-defns-
to-tenv checks each module to see whether the interface produced by
the module body matches the advertised interface, using the procedure
<:-iface.
|     |     | If it does, | the module |     | is added | to the type | environment. |     | Oth- |
| --- | --- | ----------- | ---------- | --- | -------- | ----------- | ------------ | --- | ---- |
erwise,anerrorisreported.

| 8.1 TheSimpleModuleSystem |     |     |     |     | 287 |
| ------------------------- | --- | --- | --- | --- | --- |
The interface of a module body associates each variable defined in the
body with the type of its definition. For example, if we looked atthe body
fromourfirstexample,
| [a = 33     |     |     |     |     |     |
| ----------- | --- | --- | --- | --- | --- |
| x = -(a,1)  |     |     |     |     |     |
| b = -(a,x)  |     |     |     |     |     |
| c = -(x,b)] |     |     |     |     |     |
weshouldget
| [a : int |     |     |     |     |     |
| -------- | --- | --- | --- | --- | --- |
| x : int  |     |     |     |     |     |
| b : int  |     |     |     |     |     |
| c : int] |     |     |     |     |     |
Once we build an interface describing all the bindings exported by the
modulebody,wecancompareittotheinterfacethatthemoduleadvertises.
Recallthatasimpleinterfacecontainsalistofdeclarations. Theprocedure
defns-to-decls creates such a list, calling type-of to find the type of
eachdefinition. Ateverystepitalsoextendsthelocaltypeenvironment, to
| followthecorrectlet∗ | scoping. | (Seefigure8.6.) |     |     |     |
| -------------------- | -------- | --------------- | --- | --- | --- |
Allthat’sleftistocomparetheactualandexpectedtypesofeachmodule,
using the procedure <:-iface. We intend to define <: so that if i <:i ,
1 2
thenanymodulethatsatisfiesinterfacei alsosatisfiesinterfacei .Forexam-
|     |     | 1   |     | 2   |     |
| --- | --- | --- | --- | --- | --- |
ple
| [u : int         | [u             | : int         |          |          |     |
| ---------------- | -------------- | ------------- | -------- | -------- | --- |
| v : bool         | <: z           | : int]        |          |          |     |
| z : int]         |                |               |          |          |     |
|                  |                |               | [u : int | v : bool | z : |
| since any module | that satisfies | the interface |          |          |     |
int]providesallthevaluesthatareadvertisedbytheinterface[u : int
z : int].
For our simple module language, <:-ifacejust calls<:-decls,which
comparesdeclarations. These procedurestakea tenv argumentthatis not
used for the simple module system, but will be needed in section 8.2. See
figure8.7.
The procedure <:-decls does the main work of comparing two sets of
declarations. Ifdecls anddecls aretwosetsofdeclarations,wesaydecls < :
|     | 1   | 2   |     |     | 1   |
| --- | --- | --- | --- | --- | --- |
decls ifandonlyifanymodulethatsuppliesbindingsforthedeclarationsin
2
decls 1 alsosuppliesbindingsforthedeclarationsindecls 2 .Thiscanbeassured

288 8 Modules
|              |                    |       | ×      | →       |       |     |     |
| ------------ | ------------------ | ----- | ------ | ------- | ----- | --- | --- |
| interface-of | : ModuleBody       |       | Tenv   |         | Iface |     |     |
| (define      | interface-of       |       |        |         |       |     |     |
| (lambda      | (m-body            | tenv) |        |         |       |     |     |
| (cases       | module-body        |       | m-body |         |       |     |     |
|              | (defns-module-body |       |        | (defns) |       |     |     |
(simple-iface
|                | (defns-to-decls |              |     | defns | tenv))))))   |     |     |
| -------------- | --------------- | ------------ | --- | ----- | ------------ | --- | --- |
|                |                 |              | ×   |       | →            |     |     |
| defns-to-decls | :               | Listof(Defn) |     | Tenv  | Listof(Decl) |     |     |
| (define        | defns-to-decls  |              |     |       |              |     |     |
| (lambda        | (defns          | tenv)        |     |       |              |     |     |
| (if            | (null?          | defns)       |     |       |              |     |     |
’()
|     | (cases    | definition | (car     | defns) |         |     |     |
| --- | --------- | ---------- | -------- | ------ | ------- | --- | --- |
|     | (val-defn | (var-name  |          | exp)   |         |     |     |
|     | (let      | ((ty       | (type-of | exp    | tenv))) |     |     |
(cons
|     |     | (val-decl | var-name |     | ty) |     |     |
| --- | --- | --------- | -------- | --- | --- | --- | --- |
(defns-to-decls
(cdr defns)
|     |           | (extend-tenv |                                | var-name |     | ty tenv))))))))) |     |
| --- | --------- | ------------ | ------------------------------ | -------- | --- | ---------------- | --- |
|     | Figure8.6 |              | CheckerforSIMPLE-MODULES,part2 |          |     |                  |     |
ifdecls containsamatchingdeclarationforeverydeclarationindecls ,asin
| 1   |     |     |     |     |     |     | 2   |
| --- | --- | --- | --- | --- | --- | --- | --- |
theexampleabove.
|               |     | <:-decls |       |        | decls1 | decls2. | decls2 |
| ------------- | --- | -------- | ----- | ------ | ------ | ------- | ------ |
| The procedure |     |          | first | checks |        | and     | If     |
is empty, then it makes no demands on decls1, so the answer is #t. If
decls2 is non-empty, but decls1 is empty, then decls2 requires some-
thing,butdecls1hasnothing. Sotheansweris#f. Otherwise,wecompare
thenamesofthefirstvariablesdeclaredbydecls1anddecls2. Iftheyare
thesame,thentheirtypesmustmatch,andwerecurontherestofbothlists
ofdeclarations.Iftheyarenotthesame,thenwerecuronthecdrofdecls1
tolookforsomethingthatmatchesthefirstdeclarationofdecls2.
Thiscompletesthesimplemodulesystem.

8.1 TheSimpleModuleSystem 289
|          |                | ×         | ×              | →      |             |        |     |
| -------- | -------------- | --------- | -------------- | ------ | ----------- | ------ | --- |
| <:-iface | : Iface        | Iface     | Tenv           | Bool   |             |        |     |
| (define  | <:-iface       |           |                |        |             |        |     |
| (lambda  | (iface1        |           | iface2 tenv)   |        |             |        |     |
| (cases   |                | interface | iface1         |        |             |        |     |
|          | (simple-iface  |           | (decls1)       |        |             |        |     |
|          | (cases         | interface | iface2         |        |             |        |     |
|          | (simple-iface  |           | (decls2)       |        |             |        |     |
|          |                | (<:-decls | decls1         | decls2 | tenv))))))) |        |     |
| <:-decls | : Listof(Decl) |           | × Listof(Decl) | ×      | Tenv        | → Bool |     |
| (define  | <:-decls       |           |                |        |             |        |     |
| (lambda  | (decls1        |           | decls2 tenv)   |        |             |        |     |
(cond
|     | ((null? | decls2) | #t) |     |     |     |     |
| --- | ------- | ------- | --- | --- | --- | --- | --- |
|     | ((null? | decls1) | #f) |     |     |     |     |
(else
|     | (let | ((name1 | (decl->name  |     | (car | decls1)))  |     |
| --- | ---- | ------- | ------------ | --- | ---- | ---------- | --- |
|     |      | (name2  | (decl->name  |     | (car | decls2)))) |     |
|     | (if  | (eqv?   | name1 name2) |     |      |            |     |
(and
(equal?
|     |           | (decl->type                          |      | (car    | decls1))  |              |        |
| --- | --------- | ------------------------------------ | ---- | ------- | --------- | ------------ | ------ |
|     |           | (decl->type                          |      | (car    | decls2))) |              |        |
|     |           | (<:-decls                            | (cdr | decls1) |           | (cdr decls2) | tenv)) |
|     |           | (<:-decls                            | (cdr | decls1) | decls2    | tenv)))))))  |        |
|     | Figure8.7 | ComparinginterfacesforSIMPLE-MODULES |      |         |           |              |        |
(cid:3)
Exercise8.1 [ ]Modifythecheckertodetectandrejectanyprogramthatdefinestwo
moduleswiththesamename.
(cid:3)
Exercise8.2 [ ]The procedure add-module-defn-to-env is not quite right,
becauseitaddsallthevaluesdefinedbythemodule,notjustthe onesinthe inter-
face. Modifyadd-module-defn-to-envsothat itaddstothe environmentonly
thevaluesdeclaredintheinterface.Doesadd-module-defn-to-tenvsufferfrom
thesameproblem?
(cid:3)
Exercise8.3 [ ]Changethesyntaxofthelanguagesothataqualifiedvariablerefer-
| enceappearsasm.v,ratherthanfrom |         |     |     | m take | v.  |     |     |
| ------------------------------- | ------- | --- | --- | ------ | --- | --- | --- |
|                                 | (cid:3) |     |     |        |     |     | let |
Exercise8.4 [ ]Change the expression language to include multiple declara-
tions, multiargument procedures, and multiple letrec declarations, as in exer-
cise7.24.

290 8 Modules
(cid:3)
Exercise8.5 [ ] Allow let and letrecdeclarations to be used in modulebodies.
Forexample,oneshouldbeabletowrite
module even-odd
interface
| [even : int | -> bool  |     |     |
| ----------- | -------- | --- | --- |
| odd : int   | -> bool] |     |     |
body
letrec
| bool local-odd  | (x :         | int) = ... (local-even  | -(x,1)) ... |
| --------------- | ------------ | ----------------------- | ----------- |
| bool local-even | (x           | : int) = ... (local-odd | -(x,1)) ... |
| in [even        | = local-even |                         |             |
| odd             | = local-odd] |                         |             |
(cid:3)(cid:3)
Exercise8.6 [ ] Allow local module definitions to appear in module bodies. For
example,oneshouldbeabletowrite
module m1
interface
[u : int
v : int]
body
module m2
| interface | [v : int] |     |     |
| --------- | --------- | --- | --- |
| body [v   | = 33]     |     |     |
[u = 44
| v = -(from | m2 take | v, 1)] |     |
| ---------- | ------- | ------ | --- |
Exercise8.7 [ (cid:3)(cid:3) ]Extendyoursolutiontotheprecedingexercisetoallowmodulesto
exportothermodulesascomponents.Forexample,oneshouldbeabletowrite
module m1
interface
[u : int
| n : [v : | int]] |     |     |
| -------- | ----- | --- | --- |
body
module m2
| interface | [v : int] |     |     |
| --------- | --------- | --- | --- |
| body [v   | = 33]     |     |     |
[u = 44
n = m2]
| from m1 take | n take v |     |     |
| ------------ | -------- | --- | --- |
Exercise8.8 [ (cid:3)(cid:3) ]Inourlanguage,themodulemustproducethevaluesinthesame
orderastheinterface,butthatcouldeasilybefixed.Fixit.
(cid:3)(cid:3)
Exercise8.9 [ ]Wesaidthatourmodulesystemshoulddocumentthedependen-
cies between modules. Add this capability to SIMPLE-MODULES by requiring a

8.1 TheSimpleModuleSystem 291
depends-on clause in each module body and in the program body. Rather than
havingallprecedingmodulesinscopeinamodulem,aprecedingmoduleisinscope
onlyifitislistedinm’sdepends-onclause.Forexample,considertheprogram
| module m1 | ... |     |
| --------- | --- | --- |
| module m2 | ... |     |
| module m3 | ... |     |
| module m4 | ... |     |
module m5
| interface | [...] |     |
| --------- | ----- | --- |
body
| depends-on | m1, m3 |     |
| ---------- | ------ | --- |
[...]
Inthebodyofm5,qualifiedvariableswouldbeinscopeonlyiftheycamefromm1or
m3.Areferencetofrom m4 take xwouldbeill-typed,evenifm4exportedavalue
forx.
(cid:3)(cid:3)(cid:3)
Exercise8.10 [ ] We couldalsouse afeaturelikedepends-onto controlwhen
modulebodiesareevaluated. AddthiscapabilitytoSIMPLE-MODULESbyrequir-
ing an imports clause to each module body and program body. imports is like
depends-on,buthastheadditionalpropertythatthebodyofamoduleisevaluated
onlywhenitisimportedbysomeothermodule(usinganimportsclause).
Thusifourlanguagehadprintexpressions,theprogram
module m1
| interface | [] body | [x = print(1)] |
| --------- | ------- | -------------- |
module m2
| interface | [] body | [x = print(2)] |
| --------- | ------- | -------------- |
module m3
| interface | []  |     |
| --------- | --- | --- |
body
| import | m2  |     |
| ------ | --- | --- |
[x = print(3)]
| import m3, | m1  |     |
| ---------- | --- | --- |
33
wouldprint2,3,and1beforereturning33.Herethemoduleshaveemptyinterfaces,
becauseweareonlyconcernedwiththeorderinwhichthebodiesareevaluated.
(cid:3)(cid:3)(cid:3)
Exercise8.11 [ ]ModifythecheckertouseINFERREDasthelanguageofexpres-
sions. Forthisexerciseyouwillneedto modify<:-declstousesomethingother
thanequal?tocomparetypes.Forexample,in
module m
| interface | [f : (int | -> int)] |
| --------- | --------- | -------- |
| body [f   | = proc (x | : ?) x]  |

292 8 Modules
the actual type for f reported by the type inference engine will be something like
(tvar07 -> tvar07),andthisshouldbeaccepted.Ontheotherhand,weshould
rejectthemodule
| module m  |           |           |     |
| --------- | --------- | --------- | --- |
| interface | [f : (int | -> bool)] |     |
| body [f   | = proc (x | : ?) x]   |     |
even though the type inference engine will report the same type (tvar07 ->
tvar07)forf.
| 8.2 ModulesThat | DeclareTypes |     |     |
| --------------- | ------------ | --- | --- |
Sofar,ourinterfaceshavedeclaredonlyordinaryvariablesandtheirtypes.
In the next module language, OPAQUE-TYPES, we allow interfaces to
| declaretypesaswell. | Forexample,inthedefinition |     |     |
| ------------------- | -------------------------- | --- | --- |
| module m1           |                            |     |     |
interface
[opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
| is-zero | : (t -> | bool)] |     |
| ------- | ------- | ------ | --- |
body
...
theinterfacedeclaresatypet,andsomeoperationszero,succ,pred,and
is-zerothatoperateonvaluesofthattype.
Thisistheinterfacethatmight
be associated with an implementation of arithmetic, as in section 2.1. Here
t is declared to be an opaque type, meaning that code outside the module
boundary does not know how values of this type are represented. All the
outsidecodeknowsisthatitcanmanipulatevaluesoftypefrom m1 take
| t   | from | m1 take zero, from | m1 take succ, |
| --- | ---- | ------------------ | ------------- |
with the procedures etc.
| Thusfrom m1 | take tbehaveslikeaprimitivetypesuchasintorbool. |     |     |
| ----------- | ----------------------------------------------- | --- | --- |
We will introduce two kindsof type declarations: transparentand opaque.
Botharenecessaryforagoodmodulesystem.

8.2 ModulesThatDeclareTypes 293
8.2.1 Examples
To motivate this, consider our developers again. Alice has been using a
data structure consisting of a pair of integers, representing the x- and y-
coordinates of a point. She is using a language with types like those of
exercise7.8, soher module, namedAlices-points,hasaninterfacewith
declarationslike
| initial-point |     | : (int    | ->  | pairof    | int * int) |            |
| ------------- | --- | --------- | --- | --------- | ---------- | ---------- |
| increment-x   |     | : (pairof |     | int * int | -> pairof  | int * int) |
Bob and Charlie complain about this. They don’t want to have to write
pairof int * intoverandoveragain.Alicethereforerewritesherinter-
| facetousetransparenttypedeclarations. |     |     |     | Thisallowshertowrite |     |     |
| ------------------------------------- | --- | --- | --- | -------------------- | --- | --- |
module Alices-points
interface
| [transparent  |     | point | = pairof | int       | * int |     |
| ------------- | --- | ----- | -------- | --------- | ----- | --- |
| initial-point |     | :     | (int     | -> point) |       |     |
| increment-x   |     | :     | (point   | -> point) |       |     |
| get-x         |     | :     | (point   | -> int)   |       |     |
...]
Thissimplifieshertask,sinceshehaslesswritingtodo,anditmakesher
collaborators’taskssimpler,becauseintheirimplementationstheycanwrite
definitionslike
| [transparent |      | point | = from   | Alices-points |     | take point |
| ------------ | ---- | ----- | -------- | ------------- | --- | ---------- |
| foo =        | proc | (p1 : | point)   |               |     |            |
|              | proc | (p2   | : point) | ...           |     |            |
...]
For some projects, this would do nicely. On the other hand, the points
in Alice’s project happen to represent points on a metal track with a fixed
geometry, so the x- and y-coordinates are not independent. Alice’s imple-
mentationofincrement-xcarefullyupdatesthey-coordinatetomatchthe
changeinthex-coordinate. ButBobdoesn’tknowthis,andsohewriteshis
ownprocedure
| increment-y | =   | proc          | (p : | point)   |     |     |
| ----------- | --- | ------------- | ---- | -------- | --- | --- |
|             |     | unpair        | x    | y = p    |     |     |
|             |     | in newpair(x, |      | -(y,-1)) |     |     |
Because Bob’s code changes the y-coordinate without changing the x-
coordinatecorrespondingly,Alice’scodenolongerworkscorrectly.

294 8 Modules
Worseyet,whatifAlicedecidestochangetherepresentationofpointsso
that the y-coordinate is in the first component? She can change her code
to match this new representation. But then Bob’s code would be broken,
becausehisincrement-yprocedurenowchangesthewrongcomponentof
thepair.
point
Alice can solve her problem by making an opaque data type. She
rewritesherinterfacetosay
| opaque        | point    |           |     |     |
| ------------- | -------- | --------- | --- | --- |
| initial-point | : (int   | -> point) |     |     |
| increment-x   | : (point | -> point) |     |     |
| get-x         | : (point | -> int)   |     |     |
Now Bob can create new points using the procedure initial-point,
| andhecanmanipulatepointsusingfrom |      | Alices-points |        | take get-x    |
| --------------------------------- | ---- | ------------- | ------ | ------------- |
| from Alices-points                | take | increment-x,  |        |               |
| and                               |      |               | but he | can no longer |
manipulatepointsusinganyproceduresotherthantheonesinAlice’sinter-
face.Inparticular,hecannolongerwritetheincrement-yprocedure,since
itmanipulatesapointusingsomethingotherthantheproceduresinAlice’s
interface.
Intheremainderofthissection,weexplorefurtherexamplesofthesefacil-
ities.
TransparentTypes
We begin by discussing transparent type declarations. These are sometimes
calledconcretetypedeclarationsortypeabbreviations.
Example8.6Theprogram
| module m1 |     |     |     |     |
| --------- | --- | --- | --- | --- |
interface
| [transparent | t = int |     |     |     |
| ------------ | ------- | --- | --- | --- |
z : t
s : (t -> t)
is-z? : (t -> bool)]
body
[type t = int
z = 33
| s = proc       | (x : t) -(x,-1) |                   |     |     |
| -------------- | --------------- | ----------------- | --- | --- |
| is-z?          | = proc (x :     | t) zero?(-(x,z))] |     |     |
| proc (x        | : from m1 take  | t)                |     |     |
| (from          | m1 take is-z?   | -(x,0))           |     |     |
| hastype(int -> | bool).          |                   |     |     |

| 8.2 ModulesThatDeclareTypes |     |     |     | 295 |
| --------------------------- | --- | --- | --- | --- |
Figure8.8 Scopesforamodulethatdeclarestypes
The declaration transparent t = int in the interface binds t to the
type int in the rest of the interface, so we can write z : t. More impor-
| tantly,italsobindsfrom | m1  | take ttointintherestoftheprogram. |     |     |
| ---------------------- | --- | --------------------------------- | --- | --- |
We
callthisaqualifiedtype.Herewehaveusedittodeclarethetypeofthebound
variablez. Thescopeofadeclarationistherestoftheinterfaceandtherest
oftheprogramafterthemoduledefinition.
| Thedefinitiontype  | t =      | intinthebodybindsttothetypeintinthe |                   |      |
| ------------------ | -------- | ----------------------------------- | ----------------- | ---- |
|                    |          | s                                   | = proc (x : t)... |      |
| rest of the module | body, so | we could write                      |                   | . As |
before,thescopeofadefinitionistherestofthebody(seefigure8.8).
| Ofcourse, wecanuseanynamewe |     | likeforthe | type,andwecandeclare |     |
| --------------------------- | --- | ---------- | -------------------- | --- |
morethanonetype.Thetypedeclarationscanappearanywhereintheinter-
face,solongaseachdeclarationprecedesallofitsuses.

| 296 |     |     | 8   | Modules |
| --- | --- | --- | --- | ------- |
OpaqueTypes
Amodule canalsoexportopaquetypesbyusing anopaque-typedeclara-
tion. Opaquetypesaresometimescalledabstracttypes.
Example8.7Let’staketheprograminexample8.6andreplacethe trans-
| parenttypedeclarationbyanopaqueone. |     | Theresultingprogramis |     |     |
| ----------------------------------- | --- | --------------------- | --- | --- |
| module m1                           |     |                       |     |     |
interface
[opaque t
z : t
s : (t -> t)
is-z? : (t -> bool)]
body
[type t = int
z = 33
| s = proc             | (x : t)                                | -(x,-1)             |     |     |
| -------------------- | -------------------------------------- | ------------------- | --- | --- |
| is-z?                | = proc (x                              | : t) zero?(-(x,z))] |     |     |
| proc (x              | : from m1                              | take t)             |     |     |
| (from                | m1 take is-z?                          | -(x,0))             |     |     |
| Thedeclarationopaque | tintheinterfacedeclaresttobethenameofa |                     |     |     |
newopaquetype. Anopaquetypebehaveslikeanewprimitivetype, such
asintorbool.Thenamedtypetisboundtothisopaquetypeintherestof
theinterface,andthequalifiedtypefrom m1 take tisboundtothesame
opaque type in the rest of the program. All the rest of the program knows
about the type from m1 take t is that from m1 take z is bound to a
| valueofthattype,andthatfrom |     | m1 take sandfrom | m1 take | is-z? |
| --------------------------- | --- | ---------------- | ------- | ----- |
areboundtoproceduresthatcanmanipulatevaluesofthattype. Thisisthe
abstraction boundary. The type checker guarantees that the evaluation of
an expression that has type from m1 take t is safe, so that the value of
theexpressionhasbeenconstructedonlybytheseoperators,asdiscussedon
page239.
|                   |            | type t = int definest |              |     |
| ----------------- | ---------- | --------------------- | ------------ | --- |
| The corresponding | definition |                       | to be a name | for |
int inside the module body, but this information is hidden from the rest
of the program, because the rest of the program gets its bindings from the
moduleinterface.
So -(x,0) is not well-typed, because the main program does not know
| thatvaluesoftypefrom | m1  | take tareactuallyvaluesoftypeint. |     |     |
| -------------------- | --- | --------------------------------- | --- | --- |

8.2 ModulesThatDeclareTypes 297
Let’schangetheprogramtoremovethearithmeticoperation,getting
|     | module m1 |     |     |     |     |
| --- | --------- | --- | --- | --- | --- |
interface
|     | [opaque | t   |     |     |     |
| --- | ------- | --- | --- | --- | --- |
z : t
|     | s : (t | -> t) |           |     |     |
| --- | ------ | ----- | --------- | --- | --- |
|     | is-z?  | : (t  | -> bool)] |     |     |
body
|     | [type | t = int |     |     |     |
| --- | ----- | ------- | --- | --- | --- |
z = 33
|     | s = proc | (x      | : t)    | -(x,-1)           |     |
| --- | -------- | ------- | ------- | ----------------- | --- |
|     | is-z?    | = proc  | (x :    | t) zero?(-(x,z))] |     |
|     | proc (x  | : from  | m1 take | t)                |     |
|     | (from    | m1 take | is-z?   | x)                |     |
(from m1 take t ->
| Now | we have a | well-typed | programthathas |     | type |
| --- | --------- | ---------- | -------------- | --- | ---- |
bool).
By enforcing this abstraction boundary, the type checker guarantees that
noprogrammanipulatesthevaluesprovidedbytheinterfaceexceptthrough
the procedures that the interface provides. This gives us a mechanism to
enforce the distinction between the users of a data type and its implemen-
tation,asdiscussedinchapter2. Wenextshowsomeexamplesofthistech-
nique.
Example8.8Ifaprogramusesamoduledefinition
|     | module colors |     |     |     |     |
| --- | ------------- | --- | --- | --- | --- |
interface
|     | [opaque | color    |     |           |     |
| --- | ------- | -------- | --- | --------- | --- |
|     | red :   | color    |     |           |     |
|     | green   | : color  |     |           |     |
|     | is-red? | : (color |     | -> bool)] |     |
body
|     | [type   | color  | = int |          |           |
| --- | ------- | ------ | ----- | -------- | --------- |
|     | red =   | 0      |       |          |           |
|     | green   | = 1    |       |          |           |
|     | is-red? | = proc | (c    | : color) | zero?(c)] |
there is no way the program can figure out that from colors take
color is actually int, or that from colors take green is actually 1
(except,perhaps,byreturningacolor asthe finalanswer andthenprinting
itout).

298 8 Modules
Example8.9Theprogram
module ints1
interface
[opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
is-zero : (t -> bool)]
body
[type t = int
zero = 0
succ = proc(x : t) -(x,-5)
pred = proc(x : t) -(x,5)
is-zero = proc (x : t) zero?(x)]
let z = from ints1 take zero
in let s = from ints1 take succ
in (s (s z))
hastypefrom ints1 take t. Ithasvalue10,butwecanmanipulatethis
valueonlythroughtheproceduresthatareexportedfromints1.Thismod-
∗
ule representsthe integer k by the expressed value 5 k. In the notation of
(cid:13) (cid:14)= ∗
section2.1, k 5 k.
(cid:13) (cid:14)=− ∗
Example8.10Inthismodule, k 3 k.
module ints2
interface
[opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
is-zero : (t -> bool)]
body
[type t = int
zero = 0
succ = proc(x : t) -(x,3)
pred = proc(x : t) -(x,-3)
is-zero = proc (x : t) zero?(x)]
let z = from ints2 take zero
in let s = from ints2 take succ
in (s (s z))
hastypefrom ints2 take tandhasvalue-6.

8.2 ModulesThatDeclareTypes 299
Example8.11Intheprecedingexamples,wecouldn’tmanipulatetheval-
ues directly, but we could manipulate them using the proceduresexported
bythemodule. Aswedidinchapter2,wecancomposetheseproceduresto
do useful work. Here we combine them to write a procedureto-int that
convertsavaluefromthemodulebacktoavalueoftypeint.
| module     | ints1 ...asbefore... |           |             |      |
| ---------- | -------------------- | --------- | ----------- | ---- |
| let z =    | from ints1           | take zero |             |      |
| in let     | s = from ints1       | take succ |             |      |
| in let     | p = from ints1       | take pred |             |      |
| in let     | z? = from ints1      | take      | is-zero     |      |
| in letrec  | int to-int           | (x : from | ints1 take  | t) = |
|            | if (z?               | x)        |             |      |
|            | then                 | 0         |             |      |
|            | else                 | -((to-int | (p x)), -1) |      |
| in (to-int | (s (s z)))           |           |             |      |
hastypeintandhasvalue2.
Example8.12Hereisthesametechniqueusedwiththeimplementationof
arithmeticints2.
| module     | ints2 ...asbefore... |           |            |     |
| ---------- | -------------------- | --------- | ---------- | --- |
| let z =    | from ints2           | take zero |            |     |
| in let     | s = from ints2       | take succ |            |     |
| in let     | p = from ints2       | take pred |            |     |
| in let     | z? = from ints2      | take      | is-zero    |     |
| in letrec  | int to-int           | (x : from | ints2 take | t)  |
|            | = if                 | (z? x)    |            |     |
|            | then                 | 0         |            |     |
|            | else                 | -((to-int | (p x)),    | -1) |
| in (to-int | (s (s z)))           |           |            |     |
alsohastypeintandvalue2.
Weshowinsection8.3howtoabstractoverthesetwoexamples.

300 8 Modules
Example8.13 Inthenextprogram,weconstructamoduletoencapsulate
a data type of booleans. The booleans are representedas integers, but that
factishiddenfromtherestoftheprogram,asinexample8.8.
module mybool
interface
[opaque t
true : t
false : t
and : (t -> (t -> t))
not : (t -> t)
to-bool : (t -> bool)]
body
[type t = int
true = 0
false = 13
and = proc (x : t)
proc (y : t)
if zero?(x) then y else false
not = proc (x : t)
if zero?(x) then false else true
to-bool = proc (x : t) zero?(x)]
let true = from mybool take true
in let false = from mybool take false
in let and = from mybool take and
in ((and true) false)
hastypefrom mybool take t,andhasvalue13.
(cid:3)
Exercise8.12 [ ]Inexample8.13,couldthedefinitionofandandnotbemovedfrom
insidethemoduletooutsideit?Whataboutto-bool?
(cid:3)
Exercise8.13 [ ]Writeamodulethatimplementsarithmeticusingarepresentation
inwhichtheintegerkisrepresentedas5∗k+3.

8.2 ModulesThatDeclareTypes 301
(cid:3)
Exercise8.14 [ ] Consider the following alternate definition of mybool (exam-
ple8.13):
module mybool
interface
[opaque t
true : t
false : t
and : (t -> (t -> t))
not : (t -> t)
to-bool : (t -> bool)]
body
[type t = int
true = 1
false = 0
and = proc (x : t)
proc (y : t)
if zero?(x) then false else y
not = proc (x : t)
if zero?(x) then true else false
to-bool = proc (x : t)
if zero?(x) then zero?(1) else zero?(0)]
Isthereanyprogramoftypeintthatreturnsonevalueusingtheoriginaldefinition
ofmybool,butadifferentvalueusingthenewdefinition?
(cid:3)(cid:3)
Exercise8.15 [ ] Write a module that implements a simple abstraction of tables.
Yourtablesshouldbelikeenvironments, exceptthatinsteadofbindingsymbolsto
Schemevalues,theybindintegerstointegers.Theinterfaceprovidesavaluethatrep-
resentsanemptytableandtwoproceduresadd-to-tableandlookup-in-table
that are analogous to extend-envand apply-env. Since our language has only
one-argument procedures, we get the equivalent of multiargument procedures by
using Currying (exercise 3.20). You may model the empty table with a table that
returns0foranyquery.Hereisanexampleusingthismodule.
module tables
interface
[opaque table
empty : table
add-to-table : (int -> (int -> (table -> table)))
lookup-in-table : (int -> (table -> int))]
body
[type table = (int -> int)
...]
let empty = from tables take empty
in let add-binding = from tables take add-to-table
in let lookup = from tables take lookup-in-table

302 8 Modules
| in let table1 | = (((add-binding | 3) 300) |
| ------------- | ---------------- | ------- |
|               | (((add-binding   | 4) 400) |
|               | (((add-binding   | 3) 600) |
empty)))
| in -(((lookup | 4) table1), |     |
| ------------- | ----------- | --- |
| ((lookup      | 3) table1)) |     |
Thisprogramshouldhavetypeint. Thetabletable1binds4to400and3to300,
sothevalueoftheprogramshouldbe100.
8.2.2 Implementation
We now extend our system to model transparentand opaque type declara-
tionsandqualifiedtypereferences.
SyntaxandtheInterpreter
Weaddsyntaxfortwonewkindsoftypes:namedtypes(liket)andqualified
| types(likefrom m1 | take t). |     |
| ----------------- | -------- | --- |
Type::=Identifier
|             | named-type (name) |                 |
| ----------- | ----------------- | --------------- |
| Type::=from | Identifier take   | Identifier      |
|             | qualified-type    | (m-name t-name) |
Weaddtwonewkindsofdeclarations,foropaqueandtransparenttypes.
| Decl::=opaque      | Identifier            |             |
| ------------------ | --------------------- | ----------- |
|                    | opaque-type-decl      | (t-name)    |
| Decl::=transparent | Identifier            | = Type      |
|                    | transparent-type-decl | (t-name ty) |
Wealsoaddanewkindofdefinition: atypedefinition. Thiswillbeused
todefinebothopaqueandtransparenttypes.
|     | Defn::=type Identifier | = Type    |
| --- | ---------------------- | --------- |
|     | type-defn              | (name ty) |
Theinterpreterdoesn’tlookattypesordeclarations,sotheonlychangeto
theinterpreteristomakeitignoretypedefinitions.

| 8.2          | ModulesThatDeclareTypes |              |              |     |     |     | 303 |
| ------------ | ----------------------- | ------------ | ------------ | --- | --- | --- | --- |
|              |                         |              |              | ×   | →   |     |     |
| defns-to-env |                         | :            | Listof(Defn) | Env | Env |     |     |
| (define      |                         | defns-to-env |              |     |     |     |     |
|              | (lambda                 | (defns       | env)         |     |     |     |     |
|              | (if                     | (null?       | defns)       |     |     |     |     |
(empty-env)
|     | (cases |               | definition | (car | defns)          |           |     |
| --- | ------ | ------------- | ---------- | ---- | --------------- | --------- | --- |
|     |        | (val-defn     | (var       | exp) | ...asbefore...) |           |     |
|     |        | (type-defn    | (type-name |      | type)           |           |     |
|     |        | (defns-to-env |            | (cdr | defns)          | env)))))) |     |
TheChecker
Thechangestothecheckeraremoresubstantial,sinceallthemanipulations
involvingtypesmustbeextendedtohandlethenewtypes.
First,weintroduceasystematicwayofhandlingopaqueandtransparent
types. Anopaquetypebehaveslike aprimitive type, suchasintorbool.
Transparenttypes,ontheotherhand,aretransparent,asthenamesuggests:
theybehaveexactlyliketheirdefinitions. Soeverytypeisequivalenttoone
thatisgivenbythegrammar
|     | Type::=int |     | |    | |    |        | |                |     |
| --- | ---------- | --- | ---- | ---- | ------ | ---------------- | --- |
|     |            |     | bool | from | m take | t (Type -> Type) |     |
where t is declaredas an opaque type in m. We call a type of this form an
expandedtype.
We next extend type environments to handle new types. Our type envi-
ronmentswillbindeachnamedtypeorqualifiedtypetoanexpandedtype.
Ournewdefinitionoftypeenvironmentsis
| (define-datatype |     |     | type-environment |     |     | type-environment? |     |
| ---------------- | --- | --- | ---------------- | --- | --- | ----------------- | --- |
(empty-tenv)
|     | (extend-tenv             |     | ...asbefore...) |     |                 |     |     |
| --- | ------------------------ | --- | --------------- | --- | --------------- | --- | --- |
|     | (extend-tenv-with-module |     |                 |     | ...asbefore...) |     |     |
(extend-tenv-with-type
|     | (name       | type?) |                      |     |     |     |     |
| --- | ----------- | ------ | -------------------- | --- | --- | --- | --- |
|     | (type       | type?) |                      |     |     |     |     |
|     | (saved-tenv |        | type-environment?))) |     |     |     |     |
typeisalwaysanexpandedtype.
| subjecttothecondition |     |     | that |     |     |     | Thiscondi- |
| --------------------- | --- | --- | ---- | --- | --- | --- | ---------- |
tionisaninvariant,asdiscussedonpage10.
Wenextwriteaprocedure,expand-type,whichtakesatypeandatype
environment, and which expands the type using the type bindings in the
type environment. Itlooks up named typesandqualified types inthe type
environment,relyingontheinvariantthattheresultingtypesareexpanded,
andforaproctypeitrecursontheargumentandresulttypes.

304 8 Modules
|             |             | ×              | →            |              |     |     |
| ----------- | ----------- | -------------- | ------------ | ------------ | --- | --- |
| expand-type |             | : Type         | Tenv         | ExpandedType |     |     |
| (define     | expand-type |                |              |              |     |     |
| (lambda     |             | (ty tenv)      |              |              |     |     |
|             | (cases      | type ty        |              |              |     |     |
|             | (int-type   | () (int-type)) |              |              |     |     |
|             | (bool-type  | ()             | (bool-type)) |              |     |     |
|             | (proc-type  | (arg-type      |              | result-type) |     |     |
(proc-type
|     |                                | (expand-type | arg-type    | tenv)   |               |           |
| --- | ------------------------------ | ------------ | ----------- | ------- | ------------- | --------- |
|     |                                | (expand-type | result-type |         | tenv)))       |           |
|     | (named-type                    | (name)       |             |         |               |           |
|     | (lookup-type-name-in-tenv      |              |             | tenv    | name))        |           |
|     | (qualified-type                |              | (m-name     | t-name) |               |           |
|     | (lookup-qualified-type-in-tenv |              |             |         | m-name t-name | tenv))))) |
Inordertomaintainthisinvariant,wemustbesuretocallexpand-type
| wheneverweextendthetypeenvironment. |     |     |     |     | Therearethreesuchplaces: |     |
| ----------------------------------- | --- | --- | --- | --- | ------------------------ | --- |
• intype-ofinthechecker,
whereweprocessalistofdefinitions,withdefns-to-decls,and
•
• whereweaddamoduletothetypeenvironment,inadd-module-defns-
to-tenv.
Inthechecker,wereplaceeachcalloftheform
| (extend-tenv |     | sym ty | tenv) |     |     |     |
| ------------ | --- | ------ | ----- | --- | --- | --- |
by
| (extend-tenv |     | var (expand-type |     | ty tenv) | tenv) |     |
| ------------ | --- | ---------------- | --- | -------- | ----- | --- |
In defns-to-decls, when we encounter a type definition, we expand
itsright-handsideandaddittothetypeenvironment. Thetypereturnedby
type-ofisguaranteedtobeexpanded,sowedon’tneedtoexpanditagain.
We turn a type definition into a transparent type declaration, since in the
Inadd-module-defns-to-tenv,
bodyalltypebindingsaretransparent.
we call extend-tenv-with-module, adding an interface to the type
environment. In this case we need to expand the interface to make
sure that all the types it contains are expanded. To do this, we modify
add-module-defns-to-tenvtocallexpand-iface.Seefigure8.9.
|     |           | expand-iface |     |               | expand-decls. |     |
| --- | --------- | ------------ | --- | ------------- | ------------- | --- |
| The | procedure |              |     | (figure 8.10) | calls         | We  |
separatetheseproceduresinpreparationforsection8.3.

| 8.2 ModulesThatDeclareTypes |                |                |       |        |                |     |     | 305 |
| --------------------------- | -------------- | -------------- | ----- | ------ | -------------- | --- | --- | --- |
| defns-to-decls              |                | : Listof(Defn) |       | × Tenv | → Listof(Decl) |     |     |     |
| (define                     | defns-to-decls |                |       |        |                |     |     |     |
| (lambda                     | (defns         |                | tenv) |        |                |     |     |     |
| (if                         | (null?         | defns)         |       |        |                |     |     |     |
’()
|     | (cases    | definition |           | (car         | defns)      |          |     |            |
| --- | --------- | ---------- | --------- | ------------ | ----------- | -------- | --- | ---------- |
|     | (val-defn |            | (var-name | exp)         |             |          |     |            |
|     | (let      | ((ty       | (type-of  |              | exp tenv))) |          |     |            |
|     |           | (let       | ((new-env | (extend-tenv |             | var-name |     | ty tenv))) |
(cons
|     |            | (val-decl       |       | var-name |      | ty)    |              |     |
| --- | ---------- | --------------- | ----- | -------- | ---- | ------ | ------------ | --- |
|     |            | (defns-to-decls |       |          | (cdr | defns) | new-env))))) |     |
|     | (type-defn |                 | (name | ty)      |      |        |              |     |
(let ((new-env
(extend-tenv-with-type
|     |     |     | name | (expand-type |     | ty  | tenv) tenv))) |     |
| --- | --- | --- | ---- | ------------ | --- | --- | ------------- | --- |
(cons
|                          |                          | (transparent-type-decl |       |                    |      | name   | ty)             |     |
| ------------------------ | ------------------------ | ---------------------- | ----- | ------------------ | ---- | ------ | --------------- | --- |
|                          |                          | (defns-to-decls        |       |                    | (cdr | defns) | new-env)))))))) |     |
| add-module-defns-to-tenv |                          |                        | :     | Listof(ModuleDefn) |      | ×      | Tenv → Tenv     |     |
| (define                  | add-module-defns-to-tenv |                        |       |                    |      |        |                 |     |
| (lambda                  | (defns                   |                        | tenv) |                    |      |        |                 |     |
| (if                      | (null?                   | defns)                 |       |                    |      |        |                 |     |
tenv
|     | (cases               | module-definition |                          |              | (car          | defns)         |        |         |
| --- | -------------------- | ----------------- | ------------------------ | ------------ | ------------- | -------------- | ------ | ------- |
|     | (a-module-definition |                   |                          |              | (m-name       | expected-iface |        | m-body) |
|     | (let                 | ((actual-iface    |                          |              | (interface-of |                | m-body | tenv))) |
|     |                      | (if (<:-iface     |                          | actual-iface |               | expected-iface |        | tenv)   |
|     |                      | (let              | ((new-env                |              |               |                |        |         |
|     |                      |                   | (extend-tenv-with-module |              |               |                | m-name |         |
(expand-iface
|     |     |     |     | m-name | expected-iface |     | tenv) |     |
| --- | --- | --- | --- | ------ | -------------- | --- | ----- | --- |
tenv)))
(add-module-defns-to-tenv
|     |     |     | (cdr | defns) | new-env)) |     |     |     |
| --- | --- | --- | ---- | ------ | --------- | --- | --- | --- |
(report-module-doesnt-satisfy-iface
|     |     | m-name    |                              | expected-iface |     | actual-iface)))))))) |     |     |
| --- | --- | --------- | ---------------------------- | -------------- | --- | -------------------- | --- | --- |
|     |     | Figure8.9 | CheckerforOPAQUE-TYPES,part1 |                |     |                      |     |     |

306 8 Modules
Theprocedureexpand-declsloopsthroughasetofdeclarations,creat-
inganewtypeenvironmentinwhicheverytypeorvariablenameisbound
toanexpandedtype.Onecomplicationisthatdeclarationsfollowlet∗
scop-
ing: eachdeclarationin a setof declarationsis in scope in allthe following
declarations.
Toseewhatthismeans,considerthemoduledefinition
module m1
interface
[opaque t
transparent u = int
transparent uu = (t -> u)
% point A
f : uu
...]
body
[...]
Inordertosatisfytheinvariant,m1shouldbeboundinthetypeenviron-
menttoaninterfacecontainingthedeclarations
[transparent t = from m1 take t
transparent u = int
transparent uu = (from m1 take t -> int)
f : (from m1 take t -> int)
...]
Ifwedothis,thenanytimeweretrieveatypefromthistypeenvironment,
wewillgetanexpandedtype,asdesired.
AtpointA,immediatelybeforethedeclarationoff,thetypeenvironment
shouldbind
t to from m1 take t
u to int
uu to (from m1 take t -> int)
WecallthetypeenvironmentatpointslikepointAabovetheinternaltype
environment. Thiswillbepassedasanargumenttoexpand-decls.
Wecannowwriteexpand-decls. Likedefns->decls,thisprocedure
creates only transparent declarations, since its purpose is to create a data
structureinwhichqualifiedtypescanbelookedup.
Last,we modify<:-declstohandlethe twonewkindsof declarations.
Wemustnowdealwiththescopingrelationsinsideasetofdeclarations. For
example,ifwearecomparing

8.2 ModulesThatDeclareTypes 307
|              |               | ×       | × →   |       |
| ------------ | ------------- | ------- | ----- | ----- |
| expand-iface | : Sym         | Iface   | Tenv  | Iface |
| (define      | expand-iface  |         |       |       |
| (lambda      | (m-name       | iface   | tenv) |       |
| (cases       | interface     | iface   |       |       |
|              | (simple-iface | (decls) |       |       |
(simple-iface
|              | (expand-decls      |                | m-name decls   | tenv))))))     |
| ------------ | ------------------ | -------------- | -------------- | -------------- |
| expand-decls | : Sym              | × Listof(Decl) | × Tenv         | → Listof(Decl) |
| (define      | expand-decls       |                |                |                |
| (lambda      | (m-name            | decls          | internal-tenv) |                |
| (if          | (null?             | decls) ()      |                |                |
|              | (cases declaration |                | (car decls)    |                |
|              | (opaque-type-decl  |                | (t-name)       |                |
(let ((expanded-type
|     |      | (qualified-type |     | m-name t-name))) |
| --- | ---- | --------------- | --- | ---------------- |
|     | (let | ((new-env       |     |                  |
(extend-tenv-with-type
|     |     | t-name | expanded-type | internal-tenv))) |
| --- | --- | ------ | ------------- | ---------------- |
(cons
|     |     | (transparent-type-decl |     | t-name expanded-type) |
| --- | --- | ---------------------- | --- | --------------------- |
(expand-decls
|     |                        | m-name | (cdr decls) | new-env))))) |
| --- | ---------------------- | ------ | ----------- | ------------ |
|     | (transparent-type-decl |        | (t-name     | ty)          |
(let ((expanded-type
|     |      | (expand-type | ty  | internal-tenv))) |
| --- | ---- | ------------ | --- | ---------------- |
|     | (let | ((new-env    |     |                  |
(extend-tenv-with-type
|     |     | t-name | expanded-type | internal-tenv))) |
| --- | --- | ------ | ------------- | ---------------- |
(cons
|     |     | (transparent-type-decl |     | t-name expanded-type) |
| --- | --- | ---------------------- | --- | --------------------- |
(expand-decls
|     |           | m-name    | (cdr decls) | new-env))))) |
| --- | --------- | --------- | ----------- | ------------ |
|     | (val-decl | (var-name | ty)         |              |
(let ((expanded-type
|     |     | (expand-type | ty  | internal-tenv))) |
| --- | --- | ------------ | --- | ---------------- |
(cons
|     | (val-decl |     | var-name expanded-type) |     |
| --- | --------- | --- | ----------------------- | --- |
(expand-decls
|     |            | m-name                       | (cdr decls) | internal-tenv)))))))) |
| --- | ---------- | ---------------------------- | ----------- | --------------------- |
|     | Figure8.10 | CheckerforOPAQUE-TYPES,part2 |             |                       |

308 8 Modules
|     | [transparent |      | t = int |     |     |        |     |     |
| --- | ------------ | ---- | ------- | --- | --- | ------ | --- | --- |
|     | x :          | bool |         | <:  | [y  | : int] |     |     |
|     | y :          | t]   |         |     |     |        |     |     |
when we get to the declaration of y, we need to know that t refers to the
int.
type So when we recur down the list of declarations, we need to
extendthetypeenvironmentaswego,muchaswebuiltinternal-tenvin
expand-decls. We dothis by calling extend-tenv-with-decl,which
takesadeclarationand translatesit toan appropriateextension of the type
environment(figure8.11).
decls1for
| Wealwaysuse |     |     | thisextension. |     |     | To seewhy, | considerthe | com- |
| ----------- | --- | --- | -------------- | --- | --- | ---------- | ----------- | ---- |
parison
| [transparent |      | t =    | int      |     | [opaque     |      | t                 |         |
| ------------ | ---- | ------ | -------- | --- | ----------- | ---- | ----------------- | ------- |
| transparent  |      | u =    | (t -> t) | <:  | transparent |      | u = (t            | -> int) |
| f            | : (t | -> u)] |          |     | f           | : (t | -> (int -> int))] |         |
This comparison should succeed, since a module body that supplies the
bindings on the left would be a correct implementation of the interface on
theright.
When we compare the two definitions of the type u, we need to know
|     |     | t   | int. |     |     |     |     |     |
| --- | --- | --- | ---- | --- | --- | --- | --- | --- |
that the type is in fact The same technique works even when the
declarationontheleftisnotpresentontheright,asillustratedbythedecla-
rationoftinthefirstexampleabove. Wecallexpand-typetomaintainthe
invariant that all types in the type environment are expanded. The choice
of module names in the last clause of extend-tenv-with-decl doesn’t
equal?.
matter, since the only operation on qualified types is So using
fresh-module-nameisenoughtoguaranteethatthisqualifiedtypeisnew.
| Nowwegettothekeyquestion: |     |     |     | howdowecomparedeclarations? |     |     |     | Dec- |
| ------------------------- | --- | --- | --- | --------------------------- | --- | --- | --- | ---- |
larationscanmatchonlyiftheydeclarethesamename(eitheravariableor
atype). Ifapairofdeclarationshavethesamename, thereareexactlyfour
waysinwhichtheycanmatch:
• Theyarebothvaluedeclarations,andtheirtypesmatch.
• Theyarebothopaquetypedeclarations.
• Theyarebothtransparenttypedeclarations,andtheirdefinitionsmatch.
| decl1 |     |               |      |              |     | decl2 |              |      |
| ----- | --- | ------------- | ---- | ------------ | --- | ----- | ------------ | ---- |
| •     | is  | a transparent | type | declaration, |     | and   | is an opaque | type |
declaration. For example,imagine thatour module hasaninterfacethat

| 8.2 ModulesThatDeclareTypes |                |     |                |       |        |        | 309 |
| --------------------------- | -------------- | --- | -------------- | ----- | ------ | ------ | --- |
| <:-decls                    | : Listof(Decl) |     | × Listof(Decl) |       | × Tenv | → Bool |     |
| (define                     | <:-decls       |     |                |       |        |        |     |
| (lambda                     | (decls1        |     | decls2         | tenv) |        |        |     |
(cond
|     | ((null? | decls2) | #t) |     |     |     |     |
| --- | ------- | ------- | --- | --- | --- | --- | --- |
|     | ((null? | decls1) | #f) |     |     |     |     |
(else
|     | (let | ((name1 | (decl->name |        | (car | decls1)))  |     |
| --- | ---- | ------- | ----------- | ------ | ---- | ---------- | --- |
|     |      | (name2  | (decl->name |        | (car | decls2)))) |     |
|     | (if  | (eqv?   | name1       | name2) |      |            |     |
(and
(<:-decl
|     |     | (car | decls1) | (car | decls2) | tenv) |     |
| --- | --- | ---- | ------- | ---- | ------- | ----- | --- |
(<:-decls
|     |     | (cdr | decls1) | (cdr | decls2) |     |     |
| --- | --- | ---- | ------- | ---- | ------- | --- | --- |
(extend-tenv-with-decl
|     |     |     | (car | decls1) | tenv))) |     |     |
| --- | --- | --- | ---- | ------- | ------- | --- | --- |
(<:-decls
|     |     | (cdr | decls1) | decls2 |     |     |     |
| --- | --- | ---- | ------- | ------ | --- | --- | --- |
(extend-tenv-with-decl
|                       |                        | (car  | decls1) | tenv)))))))) |      |     |     |
| --------------------- | ---------------------- | ----- | ------- | ------------ | ---- | --- | --- |
|                       |                        |       |         | ×            | →    |     |     |
| extend-tenv-with-decl |                        |       | : Decl  | Tenv         | Tenv |     |     |
| (define               | extend-tenv-with-decl  |       |         |              |      |     |     |
| (lambda               | (decl                  | tenv) |         |              |      |     |     |
|                       | (cases declaration     |       |         | decl         |      |     |     |
|                       | (val-decl              | (name | ty)     | tenv)        |      |     |     |
|                       | (transparent-type-decl |       |         | (name        | ty)  |     |     |
(extend-tenv-with-type
name
|     | (expand-type |     |     | ty tenv) |     |     |     |
| --- | ------------ | --- | --- | -------- | --- | --- | --- |
tenv))
|     | (opaque-type-decl |     |     | (name) |     |     |     |
| --- | ----------------- | --- | --- | ------ | --- | --- | --- |
(extend-tenv-with-type
name
|     | (qualified-type |     |     | (fresh-module-name |     | ’%unknown) | name) |
| --- | --------------- | --- | --- | ------------------ | --- | ---------- | ----- |
tenv)))))
|     | Figure8.11 |     | CheckerforOPAQUE-TYPES,part3 |     |     |     |     |
| --- | ---------- | --- | ---------------------------- | --- | --- | --- | --- |

310 8 Modules
| declaresopaque | tandabodythatdefinestype                         |     |     |     | t = | int.Thisshould |
| -------------- | ------------------------------------------------ | --- | --- | --- | --- | -------------- |
| beaccepted.    | Theproceduredefns-to-declsturnsthedefinitiontype |     |     |     |     |                |
t = intintoatransparenttypedeclaration,sothetest
| actual-iface |     | <: expected-iface |     |     |     |     |
| ------------ | --- | ----------------- | --- | --- | --- | --- |
inadd-module-defn-to-tenvwillaskwhether
|     | (transparent |     | t =int) | < : (opaque |     | t)  |
| --- | ------------ | --- | ------- | ----------- | --- | --- |
Sincethemoduleshouldbeaccepted,thistestshouldreturntrue.
Thistellsusthatsomethingwithaknowntypeisalwaysusableasathing
| withanunknowntype. |         | Butthereverseisfalse. |                |     | Forexample, |     |
| ------------------ | ------- | --------------------- | -------------- | --- | ----------- | --- |
|                    | (opaque |                       | < (transparent |     | =           |     |
|                    |         | t)                    | :              |     | t ty)       |     |
should be false, because the value with an opaque type may have some
actual type other than int, and a module that satisfies opaque t may
| notsatisfytransparent |     | t   | = int. |     |     |     |
| --------------------- | --- | --- | ------ | --- | --- | --- |
This gives us the code in figure 8.12. The definition of equiv-type?
expandsitstypes,sothatinexampleslike
| [transparent |     | t = int | x : bool | y : t] | <: [y | : int] |
| ------------ | --- | ------- | -------- | ------ | ----- | ------ |
above,thetontheleftwillbeexpandedtoint,andthematchwillsucceed.
(cid:3)
Exercise8.16 [ ]Extendthesystemofthissectiontousethelanguageofexercise7.24,
and then rewrite exercise 8.15 to use multiple arguments instead of procedure-
returningprocedures.
(cid:3)(cid:3)
Exercise8.17 [ ] As youdid inexercise8.8, removethe restrictionthat a module
mustproducethevaluesinthesameorderastheinterface.Remember,however,that
thedefinitionmustrespectscopingrules,especiallyfortypes.
Exercise8.18 [ (cid:3)(cid:3) ]Ourcodedependsontheinvariantthateverytypeinatypeenvi-
Weenforcethisinvariantbycallingexpand-typein
ronmentisalreadyexpanded.
many placesinthe code. Onthe other hand, itwouldbe easyto break the system
byforgettingtocallexpand-type. Refactorthecodesothattherearefewercallsto
expand-type,andtheinvariantismaintainedmorerobustly.

8.3 ModuleProcedures 311
|           | × ×          | →         |     |
| --------- | ------------ | --------- | --- |
| <:-decl : | Decl Decl    | Tenv Bool |     |
| (define   | <:-decl      |           |     |
| (lambda   | (decl1 decl2 | tenv)     |     |
(or
(and
|     | (val-decl? decl1) |     |     |
| --- | ----------------- | --- | --- |
|     | (val-decl? decl2) |     |     |
(equiv-type?
|     | (decl->type | decl1)        |     |
| --- | ----------- | ------------- | --- |
|     | (decl->type | decl2) tenv)) |     |
(and
|     | (transparent-type-decl? |     | decl1) |
| --- | ----------------------- | --- | ------ |
|     | (transparent-type-decl? |     | decl2) |
(equiv-type?
|     | (decl->type | decl1)        |     |
| --- | ----------- | ------------- | --- |
|     | (decl->type | decl2) tenv)) |     |
(and
|     | (transparent-type-decl? |         | decl1) |
| --- | ----------------------- | ------- | ------ |
|     | (opaque-type-decl?      | decl2)) |        |
(and
|             | (opaque-type-decl? | decl1)     |        |
| ----------- | ------------------ | ---------- | ------ |
|             | (opaque-type-decl? | decl2))))) |        |
| equiv-type? | : Type × Type      | × Tenv     | → Bool |
| (define     | equiv-type?        |            |        |
| (lambda     | (ty1 ty2 tenv)     |            |        |
(equal?
| (expand-type | ty1        | tenv)                        |     |
| ------------ | ---------- | ---------------------------- | --- |
| (expand-type | ty2        | tenv))))                     |     |
|              | Figure8.12 | CheckerforOPAQUE-TYPES,part4 |     |
8.3 ModuleProcedures
TheprogramsinOPAQUE-TYPEShaveafixedsetofdependencies.Perhaps
modulem4dependsonm3andm2,whichdependsonm1. Sometimeswesay
thedependenciesarehard-coded. Ingeneral,suchhard-codeddependencies
leadtobadprogramdesign,becausetheymakeitdifficulttoreusemodules.
In this section, we add to our system a facility for module procedures, some-
timescalledparameterizedmodules,thatallowmodulereuse. Wecallthenew
languagePROC-MODULES.

312 8 Modules
8.3.1 Examples
Considerourthreedevelopersagain. Charliewantstousesomeofthefacil-
itiesofAlice’smodule. ButAlice’smodule usesadatabasethatissupplied
by Bob’s module, and Charlie wants to use a different database, which is
suppliedbysomeothermodule(writtenbyDiana).
To make this possible, Alice rewritesher code using module procedures.
A module procedure is much like a procedure, except that it works with
modules,ratherthanwithexpressedvalues. Atthemodulelevel,interfaces
areliketypes. JustasthetypeofaprocedureinCHECKEDspecifiesthetype
ofitsargumentandthetypeofitsresult,theinterfaceofamoduleprocedure
specifiestheinterfaceofitsargumentandtheinterfaceofitsresult.
AlicewritesanewmoduleAlices-point-builderthatbegins
module Alices-point-builder
interface
((database : [opaque db-type
opaque node-type
insert-node : (node-type ->
(db-type -> db-type))
...])
=> [opaque point
initial-point : (int -> point)
...])
ThisinterfacesaysthatAlices-point-builderwill be amodule pro-
cedure. It will expect as an argument a module that will export two
types, db-type and node-type, a procedure insert-node, and per-
haps some other values. Given such a module, Alices-point-builder
should produce a module that exports an opaque type point, a proce-
dure initial-point, and perhaps some other values. The interface of
Alices-point-builderalso specifies a local name for its argument; we
willseelaterwhythisisnecessary.
ThebodyofAlice’snewmodulebegins
body
module-proc (m : [opaque db-type
opaque node-type
insert-node : (node-type ->
(db-type -> db-type))
...])
[type point = ...
initial-point = ... from m take insert-node ...
...]

| 8.3 | ModuleProcedures |     |     |     |     |     | 313 |
| --- | ---------------- | --- | --- | --- | --- | --- | --- |
Justasanordinaryprocedureexpressionlookslike
|     | proc | (var | : t) e |     |     |     |     |
| --- | ---- | ---- | ------ | --- | --- | --- | --- |
amoduleprocedurelookslike
|     | module-proc |     | (m : | [...]) | [...] |     |     |
| --- | ----------- | --- | ---- | ------ | ----- | --- | --- |
In this example Alice has chosen m as the name of the bound variable in
the module procedure; this need not be the same as the local name in the
interface. We repeat the interface of the argument because the scope of a
moduleinterfaceneverextendsintothemodulebody. Thiscanbefixed(see
exercise8.27).
NowAlicerebuildshermodulebywriting
|     | module | Alices-points |     |     |     |     |     |
| --- | ------ | ------------- | --- | --- | --- | --- | --- |
interface
|     | [opaque |               | point |      |           |     |     |
| --- | ------- | ------------- | ----- | ---- | --------- | --- | --- |
|     |         | initial-point | :     | (int | -> point) |     |     |
...]
body
|     | (Alices-point-builder |     |     |     | Bobs-db-Module) |     |     |
| --- | --------------------- | --- | --- | --- | --------------- | --- | --- |
andCharliebuildshismodulebywriting
|     | module | Charlies-points |     |     |     |     |     |
| --- | ------ | --------------- | --- | --- | --- | --- | --- |
interface
|     | [opaque |               | point |      |           |     |     |
| --- | ------- | ------------- | ----- | ---- | --------- | --- | --- |
|     |         | initial-point | :     | (int | -> point) |     |     |
...]
body
|     | (Alices-point-builder |     |     |     | Dianas-db-module) |     |     |
| --- | --------------------- | --- | --- | --- | ----------------- | --- | --- |
ModuleAlices-pointsusesBobs-db-moduleforthedatabase.Mod-
ule Charlies-points uses Dianas-db-module for the database. This
organizationallowsthecodeinAlices-point-buildertobeusedtwice.
Notonlydoesthisavoidhavingtowritethecodetwice,butifthecodeneeds
tobechanged,thechangescanbemadeinoneplaceandtheywillbeprop-
agatedautomaticallytobothAlices-pointsandCharlies-points.
Foranotherexample,considerexamples8.11and8.12. Inthesetwoexam-
forto-int.
| ples, | we used | what | was essentially |     | the same | code | In exam- |
| ----- | ------- | ---- | --------------- | --- | -------- | ---- | -------- |
ple8.11itwas
|     | letrec | int | to-int   | (x : from | ints1 | take t) |     |
| --- | ------ | --- | -------- | --------- | ----- | ------- | --- |
|     |        |     | = if (z? | x)        |       |         |     |
then 0
|     |     |     | else | -((to-int | (p  | x)), -1) |     |
| --- | --- | --- | ---- | --------- | --- | -------- | --- |

| 314                               |     |     |     |       |      |     | 8           | Modules |
| --------------------------------- | --- | --- | --- | ----- | ---- | --- | ----------- | ------- |
| andinexample8.12thetypeofxwasfrom |     |     |     | ints2 | take | t.  | Sowerewrite |         |
thisasamoduleparameterizedonthemodulethatproducestheintegersin
question.
Example8.14Thedeclaration
|     | module | to-int-maker |     |     |     |     |     |     |
| --- | ------ | ------------ | --- | --- | --- | --- | --- | --- |
interface
|     | ((ints | :          | [opaque t    |             |           |     |     |     |
| --- | ------ | ---------- | ------------ | ----------- | --------- | --- | --- | --- |
|     |        |            | zero : t     |             |           |     |     |     |
|     |        |            | succ : (t -> | t)          |           |     |     |     |
|     |        |            | pred : (t -> | t)          |           |     |     |     |
|     |        |            | is-zero : (t | -> bool)])  |           |     |     |     |
|     |        | => [to-int | : (from      | ints take t | -> int)]) |     |     |     |
body
|     | module-proc |     | (ints : [opaque | t   |     |     |     |     |
| --- | ----------- | --- | --------------- | --- | --- | --- | --- | --- |
zero : t
|     |     |     |     | succ : (t -> | t)         |     |     |     |
| --- | --- | --- | --- | ------------ | ---------- | --- | --- | --- |
|     |     |     |     | pred : (t -> | t)         |     |     |     |
|     |     |     |     | is-zero : (t | -> bool)]) |     |     |     |
[to-int
|     |     | = let     | z? = from ints | take is-zero   |      |      |     |     |
| --- | --- | --------- | -------------- | -------------- | ---- | ---- | --- | --- |
|     |     | in let    | p = from       | ints take pred |      |      |     |     |
|     |     | in letrec | int to-int     | (x : from      | ints | take | t)  |     |
= if (z? x)
then 0
|     |     |            |     | else -((to-int | (p  | x)), | -1) |     |
| --- | --- | ---------- | --- | -------------- | --- | ---- | --- | --- |
|     |     | in to-int] |     |                |     |      |     |     |
defines a module procedure. The interface says that this module takes as
a module ints that implements the interface of arithmetic, and produces
anothermodulethatexportsato-intprocedurethatconvertsints’stype
| t   |             |     | to-int    |           |        |        |     |        |
| --- | ----------- | --- | --------- | --------- | ------ | ------ | --- | ------ |
| to  | an integer. | The | resulting | procedure | cannot | depend |     | on the |
implementation of arithmetic, since here we don’t know what that imple-
mentationis! Inthis codeintsisdeclaredtwice: once inthe interfaceand
onceinthebody. Thisisbecause,aswesaidearlier,thescopeofthedeclara-
tionintheinterfaceislocaltotheinterface,anddoesnotincludethebodyof
themodule.

| 8.3 | ModuleProcedures |     |     |     |     |     | 315 |
| --- | ---------------- | --- | --- | --- | --- | --- | --- |
Let’slookatafewexamplesofto-intinaction:
Example8.15
|     | module    | to-int-maker |                | ...asbefore... |        |          |     |
| --- | --------- | ------------ | -------------- | -------------- | ------ | -------- | --- |
|     | module    | ints1        | ...asbefore... |                |        |          |     |
|     | module    | ints1-to-int |                |                |        |          |     |
|     | interface | [to-int      | :              | (from ints1    | take t | -> int)] |     |
body
|     | (to-int-maker |              | ints1)     |             |     |     |     |
| --- | ------------- | ------------ | ---------- | ----------- | --- | --- | --- |
|     | let two1      | = (from      | ints1      | take succ   |     |     |     |
|     |               | (from        | ints1      | take succ   |     |     |     |
|     |               |              | from ints1 | take zero)) |     |     |     |
|     | in (from      | ints1-to-int |            | take to-int |     |     |     |
two1)
hastypeintandvalue2.Herewefirstdefinethemodulesto-int-maker,
and ints1. Then we apply to-int-makerto ints1, getting the module
| ints1-to-int,whichexportsabindingforfrom |     |     |     |     | ints1-to-int |     | take |
| ---------------------------------------- | --- | --- | --- | --- | ------------ | --- | ---- |
to-int.
Here’sanexampleofto-int-makerusedtwice,fortwodifferentimple-
mentationsofarithmetic.
Example8.16
|     | module | to-int-maker |     | ...asbefore... |     |     |     |
| --- | ------ | ------------ | --- | -------------- | --- | --- | --- |
|     | module | ints1        |     |                |     |     |     |
...asbefore...
|     | module             | ints2        | ...asbefore... |              |        |          |     |
| --- | ------------------ | ------------ | -------------- | ------------ | ------ | -------- | --- |
|     | module             | ints1-to-int |                |              |        |          |     |
|     | interface          | [to-int      | :              | (from ints1  | take t | -> int)] |     |
|     | body (to-int-maker |              |                | ints1)       |        |          |     |
|     | module             | ints2-to-int |                |              |        |          |     |
|     | interface          | [to-int      | :              | (from ints2  | take t | -> int)] |     |
|     | body (to-int-maker |              |                | ints2)       |        |          |     |
|     | let s1             | = from       | ints1          | take succ    |        |          |     |
|     | in let             | z1 = from    | ints1          | take zero    |        |          |     |
|     | in let             | to-ints1     | = from         | ints1-to-int | take   | to-int   |     |

316 8 Modules
in let s2 = from ints2 take succ
in let z2 = from ints2 take zero
in let to-ints2 = from ints2-to-int take to-int
in let two1 = (s1 (s1 z1))
in let two2 = (s2 (s2 z2))
in -((to-ints1 two1), (to-ints2 two2))
has type int and value 0. If we had replaced (to-ints2 two2)
by (to-ints2 two1), the program would not be well-typed, because
to-ints2 expects an argument from the ints2 representation of arith-
metic,andtwo1isavaluefromtheints1representationofarithmetic.
(cid:3)
Exercise8.19 [ ]Thecodeforcreatingtwo1andtwo2inexample8.16isrepetitive
andthereforereadyforabstraction.Completethedefinitionofamodule
module from-int-maker
interface
((ints : [opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
is-zero : (t -> bool)])
=> [from-int : (int -> from ints take t)])
body
...
that converts an integer expressedvalue to its representationin the module ints.
Useyourmoduletoreproducethecomputationofexample8.16. Useanargument
biggerthantwo.
(cid:3)
Exercise8.20 [ ]Completethedefinitionofthemodule
module sum-prod-maker
interface
((ints : [opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
is-zero : (t -> bool)])
=> [plus : (from ints take t
-> (from ints take t
-> from ints take t))
times : (from ints take t
-> (from ints take t
-> from ints take t))])
body
[plus = ...
times = ...]

8.3 ModuleProcedures 317
to define a module procedurethat takes an implementation of arithmetic and pro-
duces sum and product proceduresfor that implementation. Use the definition of
plusfrompage33,andsomethingsimilarfortimes.
(cid:3)
Exercise8.21 [ ]Write a module procedure that takes an implementation of arith-
meticintsandproducesanotherimplementationofarithmeticinwhichthenumber
kisrepresentedbytherepresentationof2∗kinints.
(cid:3)
| Exercise8.22 [ ]Completethedefinitionofthemodule |     |     |     |
| ------------------------------------------------ | --- | --- | --- |
| module equality-maker                            |     |     |     |
interface
((ints : [opaque t
zero : t
|           | succ : (t -> | t)          |     |
| --------- | ------------ | ----------- | --- |
|           | pred : (t -> | t)          |     |
|           | is-zero : (t | -> bool)])  |     |
| => [equal | : (from ints | take t      |     |
|           | -> (from     | ints take t |     |
|           | ->           | bool))])    |     |
body
...
to define a module procedurethat takes an implementation of arithmetic and pro-
ducesanequalityprocedureforthatimplementation.
(cid:3) ]Writeamoduletable-ofthatissimilartothetablesmoduleof
Exercise8.23 [
exercise8.15,exceptthatitisparameterizedoveritscontents,soonecouldwrite
| module mybool-tables |     |     |     |
| -------------------- | --- | --- | --- |
interface
[opaque table
empty : table
| add-to-table    | : (int | ->            |     |
| --------------- | ------ | ------------- | --- |
|                 | (from  | mybool take t | ->  |
|                 | (table | -> table)))   |     |
| lookup-in-table | : (int | ->            |     |
(table ->
|     |     | from mybool take | t))] |
| --- | --- | ---------------- | ---- |
body
(table-of mybool)
| todefineatablecontainingvaluesoftypefrom |     | mybool take | t.  |
| ---------------------------------------- | --- | ----------- | --- |

318 8 Modules
8.3.2 Implementation
Syntax
Addingmoduleprocedurestoourlanguageismuchlikeaddingprocedures.
Amoduleprocedurehasaninterfacethatismuchlikeaproctype.
| Iface::=((Identifier |     |            | : Iface)    | =>Iface | )   |             |               |     |
| -------------------- | --- | ---------- | ----------- | ------- | --- | ----------- | ------------- | --- |
|                      |     | proc-iface | (param-name |         |     | param-iface | result-iface) |     |
Although this interface looks a little like an ordinary procedure type, it
is different in two ways. First, it describes functions from module values
to module values, rather than from expressed values to expressed values.
Second, unlike a procedure type, it gives a name to the input to the func-
tion. This is necessary because the interface of the output may depend on
thevalueoftheinput,asinthetypeofto-int-maker:
|     | ((ints | :       | [opaque | t     |            |     |           |     |
| --- | ------ | ------- | ------- | ----- | ---------- | --- | --------- | --- |
|     |        |         | zero :  | t     |            |     |           |     |
|     |        |         | succ :  | (t -> | t)         |     |           |     |
|     |        |         | pred :  | (t -> | t)         |     |           |     |
|     |        |         | is-zero | : (t  | -> bool)]) |     |           |     |
|     | =>     | [to-int | : (from | ints  | take       | t   | -> int)]) |     |
to-int-maker takes a module ints and produces a module whose type
depends not just on the type of ints, which is fixed, but on ints itself.
When we apply to-int-maker to ints1, as we did in example 8.16, we
getamodulewithinterface
|     | [to-int | :   | (from ints1 |     | take | t -> | int)] |     |
| --- | ------- | --- | ----------- | --- | ---- | ---- | ----- | --- |
butwhenweapplyittoints2,wegetamodulewithadifferentinterface
|     | [to-int | :   | (from ints2 |     | take | t -> | int)] |     |
| --- | ------- | --- | ----------- | --- | ---- | ---- | ----- | --- |
expand-iface
| We  | extend |     |     | to  | treat | these | new interfaces | as already |
| --- | ------ | --- | --- | --- | ----- | ----- | -------------- | ---------- |
expanded. This works because the parameter and result interfaces will be
expandedwhenneeded.
| expand-iface |               | :            | Sym × Iface | ×     | Tenv            | → Iface |               |     |
| ------------ | ------------- | ------------ | ----------- | ----- | --------------- | ------- | ------------- | --- |
| (define      |               | expand-iface |             |       |                 |         |               |     |
|              | (lambda       | (m-name      | iface       | tenv) |                 |         |               |     |
|              | (cases        | interface    |             | iface |                 |         |               |     |
|              | (simple-iface |              | (decls)     |       | ...asbefore...) |         |               |     |
|              | (proc-iface   |              | (param-name |       | param-iface     |         | result-iface) |     |
iface))))

8.3 ModuleProcedures 319
We will need newkinds of module bodiesto createa module procedure,
to refer to the bound variable of a module procedure, and to apply such a
procedure.
| ModuleBody::=module-proc |                  | (Identifier | : Iface) ModuleBody |         |
| ------------------------ | ---------------- | ----------- | ------------------- | ------- |
|                          | proc-module-body | (m-name     | m-type              | m-body) |
ModuleBody::=Identifier
|                          | var-module-body | (m-name) |       |     |
| ------------------------ | --------------- | -------- | ----- | --- |
| ModuleBody::=(Identifier | Identifier)     |          |       |     |
|                          | app-module-body | (rator   | rand) |     |
TheInterpreter
Wefirstaddanewkindofmodule,analogoustoaprocedure.
| (define-datatype | typed-module | typed-module? |     |     |
| ---------------- | ------------ | ------------- | --- | --- |
(simple-module
| (bindings | environment?)) |     |     |     |
| --------- | -------------- | --- | --- | --- |
(proc-module
| (b-var symbol?)     |                 |     |     |     |
| ------------------- | --------------- | --- | --- | --- |
| (body module-body?) |                 |     |     |     |
| (saved-env          | environment?))) |     |     |     |
We extend value-of-module-bodyto handle the new possibilities for
amodulebody. Thecodeismuchlikethatforvariablereferencesandproce-
durecallsinexpressions(figure8.13).
TheChecker
We can write down rules like the ones in section 7.2 for our new kinds
of module bodies. These rules are shown in figure 8.14. We write
| (cid:7) |     |     |     | =   |
| ------- | --- | --- | --- | --- |
( body tenv) = i instead of (interface-of body tenv) i in order to
maketherulesfitonthepage.
| Amodulevariablegetsitstypefromthetypeenvironment, |     |     |     | asonemight |
| ------------------------------------------------- | --- | --- | --- | ---------- |
expect.Amodule-procgetsitstypefromthetypeofitsparameterandthe
typeofitsbody,justliketheproceduresinCHECKED.
Anapplicationofamoduleprocedureistreatedmuchlikeaprocedurecall
inCHECKED.Buttherearetwoimportantdifferences.
First, the type of the operand (i in the rule IFACE-M-APP) need not be
2
<
exactlythesameastheparametertype(i ). Werequireonlythati :i . This
|     |     | 1   |     | 2 1 |
| --- | --- | --- | --- | --- |
issufficient,sincei < :i impliesthatanymodulethatsatisfiestheinterface
2 1
i alsosatisfiestheinterfacei ,andisthereforeanacceptableargumenttothe
| 2   | 1   |     |     |     |
| --- | --- | --- | --- | --- |
moduleprocedure.

320 8 Modules
value-of-module-body : ModuleBody × Env → TypedModule
(define value-of-module-body
(lambda (m-body env)
(cases module-body m-body
(defns-module-body (defns) ...asbefore...)
(var-module-body (m-name)
(lookup-module-name-in-env m-name env))
(proc-module-body (m-name m-type m-body)
(proc-module m-name m-body env))
(app-module-body (rator rand)
(let ((rator-val
(lookup-module-name-in-env rator env))
(rand-val
(lookup-module-name-in-env rand env)))
(cases typed-module rator-val
(proc-module (m-name m-body env)
(value-of-module-body m-body
(extend-env-with-module
m-name rand-val env)))
(else
(report-bad-module-app rator-val))))))))
Figure8.13 value-of-module-body
IFACE-M-VAR
( (cid:7) m tenv) =tenv (m)
IFACE-M-PROC
( (cid:7) body [m=i ]tenv) = i (cid:12)
1 1
( (cid:7) (m-proc (m:i ) body) tenv) = ((m:i ) => i (cid:12) )
1 1 1
IFACE-M-APP
tenv(m ) =((m:i ) => i (cid:12) ) tenv(m ) = i
1 1 1 2 2
<
i :i
2 1
( (cid:7) (m m ) tenv) = i (cid:12) [m / m]
1 2 1 2
Figure8.14 Rulesfortypingnewmodulebodies

8.3 ModuleProcedures 321
(cid:12)
Second, we substitute the operand m for m in the result type i . Con-
2 1
sider the example on page 318, where we applied the module procedure
to-int-maker,whichhastheinterface
((ints : [opaque t
zero : t
succ : (t -> t)
pred : (t -> t)
is-zero : (t -> bool)])
=> [to-int : (from ints take t -> int)])
toints1andints2.Whenweapplyto-int-makertoints1,thesubsti-
tutiongivesustheinterface
[to-int : (from ints1 take t -> int)]
Whenweapplyittoints2,thesubstitutiongivestheinterfaces
[to-int : (from ints2 take t -> int)]
asdesired.
Fromtheserules,itiseasytowritedownthecodeforinterface-of(fig-
ure8.15). Whenwecheckthebodyofamodule-proc,weaddtheparam-
etertothetypeenvironmentasifithadbeenatop-levelmodule. Thiscode
uses the procedure rename-in-iface to perform the substitution on the
resultinterface.
Last,weextend<:-ifacetohandlethenewtypes. Theruleforcompar-
ingproc-ifacesis
< (cid:12) (cid:12)/ < (cid:12) (cid:12)/ (cid:12) (cid:12) (cid:12)
i :i i [m m ] :i [m m ] m notini ori
2 1 1 1 2 2 1 2
((m :i ) => i (cid:12) )< :((m :i ) => i (cid:12) )
1 1 1 2 2 2
In order to have ((m :i ) => i (cid:12))< :((m :i ) => i (cid:12)), it must be the
1 1 1 2 2 2
casethatanymodulem thatsatisfiesthefirstinterfacealsosatisfiesthesec-
0
ondinterface.Thismeansthatanymodulewithinterfacei canbepassedas
2
(cid:12)
anargumenttom ,andanymodulethatm produceswillsatisfyi .
0 0 2
<
For the first requirement, we insist that i :i . This guaranteesthatany
2 1
modulethatsatisfiesi canbepassedasanargumenttom . Notetherever-
2 0
sal: wesaythatsubtypingiscontravariantintheparametertype.
(cid:12) < (cid:12)
Whatabouttheresulttypes? Wemightrequirethati :i . Unfortunately,
1 2
(cid:12)
thisdoesn’tquitework. i mayhaveinstancesofthemodulevariablem in
1 1
(cid:12)
it, and i may have instances of m in it. So to compare them, we rename
2 2
(cid:12)
both m and m tosome newmodule variablem. Oncewedothat,we can
1 2
(cid:12) (cid:12)/ < (cid:12) (cid:12)/
comparethemsensibly. Thisleadstotherequirementi [m m ] :i [m m ].
1 1 2 2

322 8 Modules
interface-of : ModuleBody × Tenv → Iface
(define interface-of
(lambda (m-body tenv)
(cases module-body m-body
(var-module-body (m-name)
(lookup-module-name-in-tenv tenv m-name))
(defns-module-body (defns)
(simple-iface
(defns-to-decls defns tenv)))
(app-module-body (rator-id rand-id)
(let ((rator-iface
(lookup-module-name-in-tenv tenv rator-id))
(rand-iface
(lookup-module-name-in-tenv tenv rand-id)))
(cases interface rator-iface
(simple-iface (decls)
(report-attempt-to-apply-simple-module rator-id))
(proc-iface (param-name param-iface result-iface)
(if (<:-iface rand-iface param-iface tenv)
(rename-in-iface
result-iface param-name rand-id)
(report-bad-module-application-error
param-iface rand-iface m-body))))))
(proc-module-body (rand-name rand-iface m-body)
(let ((body-iface
(interface-of m-body
(extend-tenv-with-module rand-name
(expand-iface rand-name rand-iface tenv)
tenv))))
(proc-iface rand-name rand-iface body-iface))))))
Figure8.15 CheckerforPROC-MODULES,part1
Thecodetodecidethisrelationisrelativelystraightforward(figure8.16).
(cid:12) (cid:12)/ < (cid:12) (cid:12)/
When deciding i [m m ] : i [m m ] we extend the type environment to
1 1 2 2
(cid:12) (cid:12)
provideabindingform. Weassociatem withi ,sinceithasfewercompo-
1
nentsthan i . When we callextend-tenv-with-moduleto comparethe
2
resulttypes,wecallexpand-ifacetomaintaintheinvariant.
Andnow we’redone. Gohave asundae, with anything thatsatisfiesthe
icecreaminterface,anythingthatsatisfiesthehot-toppinginterface,andany-
thing that satisfies the nuts interface. Don’t worry about how any of the
piecesareconstructed,solongastheytastegood!

8.3 ModuleProcedures 323
<:-iface : Iface × Iface × Tenv → Bool
(define <:-iface
(lambda (iface1 iface2 tenv)
(cases interface iface1
(simple-iface (decls1)
(cases interface iface2
(simple-iface (decls2)
(<:-decls decls1 decls2 tenv))
(proc-iface (param-name2 param-iface2 result-iface2)
#f)))
(proc-iface (param-name1 param-iface1 result-iface1)
(cases interface iface2
(simple-iface (decls2) #f)
(proc-iface (param-name2 param-iface2 result-iface2)
(let ((new-name (fresh-module-name param-name1)))
(let ((result-iface1
(rename-in-iface
result-iface1 param-name1 new-name))
(result-iface2
(rename-in-iface
result-iface2 param-name2 new-name)))
(and
(<:-iface param-iface2 param-iface1 tenv)
(<:-iface result-iface1 result-iface2
(extend-tenv-with-module
new-name
(expand-iface new-name param-iface1 tenv)
tenv)))))))))))
Figure8.16 CheckerforPROC-MODULES,part2
(cid:3)
Exercise8.24 [ ]Application of modules is currently allowed only for identifiers.
Whatgoeswrongwiththetyperuleforapplicationifwetrytocheckanapplication
like(m1 (m2 m3))?
(cid:3)
Exercise8.25 [ ]ExtendPROC-MODULESsothatamodulecantakemultipleargu-
ments,analogouslytoexercise3.21.

324 8 Modules
(cid:3)(cid:3)
Exercise8.26 [ ]Extendthelanguageofmodulebodiestoreplacetheproduction
formoduleapplicationby
| ModuleBody::=(ModuleBody |                 | ModuleBody) |              |
| ------------------------ | --------------- | ----------- | ------------ |
|                          | app-module-body |             | (rator rand) |
(cid:3)(cid:3)(cid:3)
Exercise8.27 [ ]InPROC-MODULES,wewinduphavingtowriteinterfaceslike
| [opaque | t              |     |     |
| ------- | -------------- | --- | --- |
| zero :  | t              |     |     |
| succ :  | (t -> t)       |     |     |
| pred :  | (t -> t)       |     |     |
| is-zero | : (t -> bool)] |     |     |
overand overagain. Addto the grammarforprogramsafacilityfornamed inter-
faces,sowecouldwrite
| interface | int-interface | = [opaque | t              |
| --------- | ------------- | --------- | -------------- |
|           |               | zero      | : t            |
|           |               | succ      | : (t -> t)     |
|           |               | pred      | : (t -> t)     |
|           |               | is-zero   | : (t -> bool)] |
module make-to-int
interface
| ((ints     | : int-interface) |           |            |
| ---------- | ---------------- | --------- | ---------- |
| => [to-int | : from           | ints take | t -> int]) |
body
...

9
Objects and Classes
Manyprogrammingtasksrequiretheprogramtomanagesomepieceofstate
through an interface. For example, a file system has internal state, but we
accessandmodifythatstateonlythroughthefilesysteminterface.Often,the
pieceofstatespansseveralvariables,andchangestothosevariablesmustbe
coordinatedinordertomaintaintheconsistencyofthestate. Onetherefore
needs some technology to ensure that the various variables that constitute
the state are updatedin a coordinated manner. Object-oriented programming
isausefultechnologyforaccomplishingthistask.
In object-oriented programming, each managed piece of state is called
an object. An object consists of several stored quantities, called its fields,
with associated procedures, called methods, that have access to the fields.
The operation of calling a method is often viewed as sending the method
nameandargumentsasamessagetotheobject;thisissometimescalledthe
message-passingviewofobject-orientedprogramming.
Procedures in stateful languages, like those in chapter 4 give another
exampleofthepowerofprogrammingwithobjects. Aprocedureisanobject
whose state is contained in its freevariables. A closure has a single behav-
ior: itmaybeinvokedonsomearguments. Forexample,theproceduregon
page105controlsthestateofacounter, andtheonlythingone candowith
thisstateistoincrementit. Moreoften,however,onewantsanobjecttohave
severalbehaviors.Object-orientedprogramminglanguagesprovidesupport
forthisability.

326 9 ObjectsandClasses
Often, one needs to manage several pieces of state with the same meth-
ods. Forexample,onemighthaveseveralfilesystemsorseveralqueuesina
program. Tofacilitatethesharingofmethods,object-orientedprogramming
systemstypicallyprovideclasses,whicharestructuresthatspecifythefields
andmethodsofeachsuchobject. Eachobjectiscreatedasaclassinstance.
Similarly, one may often have several classes with fields and methods
that are similar but not identical. To facilitate the sharing of implementa-
tion, object-oriented languages typically provide inheritance, which allows
theprogrammertodefineanewclassasasmallmodificationofanexisting
class by adding or changing the behavior of some methods, or by adding
fields. Inthiscase,wesaythenewclassinheritsfromorextendstheoldclass,
sincetherestoftheclass’sbehaviorisinheritedfromtheoriginalclass.
Whether program elements are modeling real-world objects or artificial
aspectsofasystem’sstate,aprogram’sstructureisoftenclarifiedifitcanbe
composedofobjectsthatcombinebothbehaviorandstate. Itisalsonatural
toassociatebehaviorallysimilarobjectswiththesameclass.
Real-worldobjectstypicallyhavesomestateandsomebehaviorthateither
controlsoriscontrolledbythatstate. Forexample,catscaneat,purr,jump,
andliedown,andtheseactivitiesarecontrolledbytheircurrentstate,includ-
inghowhungryandtiredtheyare.
Objects and modules have many similarities, but they are very different.
Bothmodules and classesprovide a mechanism for defining opaque types.
However, anobject is a datastructure with behavior; a module is just a set
of bindings. One may have many objects of the same class; most module
systemsdonotofferasimilarcapability.Ontheotherhand,modulesystems
suchasPROC-MODULESallowamuchmoreflexiblewayofcontrollingthe
visibilityofnames. Modulesandclassescanworkfruitfullytogether.
9.1 Object-OrientedProgramming
In this chapter, we study a simple object-oriented language that we call
CLASSES.ACLASSESprogramconsistsof asequenceof classdeclarations
followedbyanexpressionthatmaymakeuseofthoseclasses.
Figure 9.1 shows a simple program in this language. It defines c1 as a
class that inherits from object. Each object of class c1 will contain two
fieldsnamediandj. Thefieldsarecalledmembersorinstancevariables. The
class c1 supports three methods, sometimes called member functions, named
initialize,countup,andgetstate. Eachmethodconsistsofitsmethod

9.1 Object-OrientedProgramming 327
class c1 extends object
field i
field j
method initialize (x)
begin
set i = x;
set j = -(0,x)
end
method countup (d)
begin
set i = +(i,d);
set j = -(j,d)
end
method getstate () list(i,j)
let t1 = 0
t2 = 0
o1 = new c1(3)
in begin
set t1 = send o1 getstate();
send o1 countup(2);
set t2 = send o1 getstate();
list(t1,t2)
end
Figure9.1 Asimpleobject-orientedprogram
name,itsmethodvars(alsocalledmethodparameters),anditsmethodbody. The
methodnamescorrespondtothekindsofmessagestowhichinstancesofc1
canrespond. Wesometimesreferto“c1’scountupmethod.”
In this example, each of the methods of the class maintains the integrity
=−
constraint or invariant that i j. A realprogramming examplewould, of
course,likelyhavefarmorecomplexintegrityconstraints.
The program in figure 9.1 first initializes three variables. t1 and t2 are
initialized to zero. o1 is initialized toan objectof the class c1. We say this
object is an instance of class c1. An object is created using the new opera-
tion. Thiscausestheclass’sinitializemethodtobeinvoked,inthiscase
setting the object’s field i to 3 and its field j to -3. The program then calls
the getstate method of o1, returning the list (3 -3). Next, it calls o1’s
countupmethod,changingthevalueofthetwofieldsto5and-5. Thenthe

328 9 ObjectsandClasses
|     | class interior-node |     | extends object |     |
| --- | ------------------- | --- | -------------- | --- |
|     | field left          |     |                |     |
|     | field right         |     |                |     |
|     | method initialize   |     | (l, r)         |     |
begin
|     | set left  | = l; |     |     |
| --- | --------- | ---- | --- | --- |
|     | set right | = r  |     |     |
end
|     | method sum        | () +(send      | left sum(),send | right sum()) |
| --- | ----------------- | -------------- | --------------- | ------------ |
|     | class leaf-node   |                | extends object  |              |
|     | field value       |                |                 |              |
|     | method initialize |                | (v) set value   | = v          |
|     | method sum        | () value       |                 |              |
|     | let o1 = new      | interior-node( |                 |              |
new interior-node(
new leaf-node(3),
new leaf-node(4)),
new leaf-node(5))
|     | in send o1 | sum()                                            |     |     |
| --- | ---------- | ------------------------------------------------ | --- | --- |
|     | Figure9.2  | Object-orientedprogramforsummingtheleavesofatree |     |     |
getstatemethodiscalledagain,returningthelist(5 -5). Last,thevalue
| oflist(t1,t2),which |     | is((3 | -3) (5 -5)), |     |
| ------------------- | --- | ----- | ------------ | --- |
isreturnedasthe valueof
theentireprogram.
Theprograminfigure9.2illustratesakeyideainobject-orientedprogram-
ming: dynamic dispatch. In this program we have trees with two kinds of
nodes, interior-nodeandleaf-node. Tofind the sum of the leavesof
| anode, | wesenditthesummessage. |     | Generally,wedonotknowwhatkind |     |
| ------ | ---------------------- | --- | ----------------------------- | --- |
ofnodewearesendingthemessageto. Instead,eachnodeacceptsthe sum
messageandusesitssummethodtodotherightthing. Thisiscalleddynamic
dispatch. Heretheexpressionbuildsatreewithtwointeriornodesandthree
| leafnodes. | Itsendsasummessagetothenodeo1; |     |     | o1sendssummessages |
| ---------- | ------------------------------ | --- | --- | ------------------ |
toitssubtrees,andsoon,returning12attheend. Thisprogramalsoshows
thatallmethodsaremutuallyrecursive.
Amethodbodycaninvokeothermethodsofthesameobjectbyusingthe
identifierself(sometimescalledthis),whichisalwaysboundtotheobject
| onwhichthemethodhasbeeninvoked. |     |     | Forexample,in |     |
| ------------------------------- | --- | --- | ------------- | --- |

| 9.2 Inheritance |                   |           |        |      |                   |     |     | 329 |
| --------------- | ----------------- | --------- | ------ | ---- | ----------------- | --- | --- | --- |
|                 | class oddeven     | extends   | object |      |                   |     |     |     |
|                 | method initialize |           | () 1   |      |                   |     |     |     |
|                 | method even       | (n)       |        |      |                   |     |     |     |
|                 | if zero?(n)       | then      | 1 else | send | self odd(-(n,1))  |     |     |     |
|                 | method odd        | (n)       |        |      |                   |     |     |     |
|                 | if zero?(n)       | then      | 0 else | send | self even(-(n,1)) |     |     |     |
|                 | let o1 = new      | oddeven() |        |      |                   |     |     |     |
|                 | in send o1        | odd(13)   |        |      |                   |     |     |     |
themethodsevenandoddinvokeeachotherrecursively,becausewhenthey
are executed, self is bound to an object that contains them both. This is
muchlikethedynamic-bindingimplementationofrecursioninexercise3.37.
9.2 Inheritance
Inheritance allows the programmer to define new classes by incremental
modification of old ones. This is extremely useful in practice. For exam-
ple, a colored point is like a point, except that it has additional methods to
manipulateitscolor,asintheclassicexampleinfigure9.3.
If class c extends class c , we say that c is the parent or superclass of c
|     | 2   |     | 1   |     | 1   |     |     | 2   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
or that c 2 is a child of c 1 . Since inheritance defines c 2 as an extension of c 1 ,
c mustbe defined beforec . To getthings started, the languageincludes a
| 1   |     |     | 2   |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
predefinedclasscalledobjectwithnomethodsorfields.Sinceobjecthas
noinitializemethod,itisimpossibletocreateanobjectofclassobject.
Each class other than object has a single parent, but it may have many
extends
children. Thus the relation imposes a tree structure on the set of
classes,withobjectattheroot. Sinceeachclasshasatmostoneimmediate
superclass,thisisasingle-inheritancelanguage.Somelanguagesallowclasses
to inherit from multiple superclasses. Such multipleinheritance is powerful,
butitisalsoproblematic;weconsidersomeofthedifficultiesintheexercises.
Thegenealogicalanalogyisthesourceoftheterminheritance. Theanalogy
isoftenpursuedsothatwespeakoftheancestorsofaclass(thechainfroma
class’sparenttotherootclassobject)oritsdescendants.
Ifc 2 isadescendant
| ofc ,wesometimessaythatc |     |     | isasubclassofc |     | ,andwritec | <   | c . |     |
| ------------------------ | --- | --- | -------------- | --- | ---------- | --- | --- | --- |
| 1                        |     |     | 2              |     | 1          | 2   | 1   |     |
If class c inherits from class c , all the fields and methods of c will be
|     | 2   |     | 1   |     |     |     | 1   |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
visiblefromthemethodsofc ,unlesstheyareredeclaredinc . Sinceaclass
|     |     |     | 2   |     |     | 2   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
inherits all the methods and fields of its parent, an instance of a child class
can be used anywhere an instance of its parentcan be used. Similarly, any
instance of any descendant of a class can be used anywhere an instance of
theclasscanbeused. Thisissometimescalledsubclasspolymorphism. Thisis

330 9 ObjectsandClasses
| class | point extends | object |     |     |
| ----- | ------------- | ------ | --- | --- |
field x
field y
| method | initialize | (initx, inity) |     |     |
| ------ | ---------- | -------------- | --- | --- |
begin
| set | x = initx; |     |     |     |
| --- | ---------- | --- | --- | --- |
| set | y = inity  |     |     |     |
end
| method | move (dx, | dy) |     |     |
| ------ | --------- | --- | --- | --- |
begin
| set | x = +(x,dx); |     |     |     |
| --- | ------------ | --- | --- | --- |
| set | y = +(y,dy)  |     |     |     |
end
| method | get-location | () list(x,y)  |     |     |
| ------ | ------------ | ------------- | --- | --- |
| class  | colorpoint   | extends point |     |     |
field color
| method | set-color               | (c) set color | = c |     |
| ------ | ----------------------- | ------------- | --- | --- |
| method | get-color               | () color      |     |     |
| let p  | = new point(3,4)        |               |     |     |
| cp     | = new colorpoint(10,20) |               |     |     |
in begin
| send      | p move(3,4);      |                 |           |         |
| --------- | ----------------- | --------------- | --------- | ------- |
| send      | cp set-color(87); |                 |           |         |
| send      | cp move(10,20);   |                 |           |         |
| list(send | p                 | get-location(), | % returns | (6 8)   |
|           | send cp           | get-location(), | % returns | (20 40) |
|           | send cp           | get-color())    | % returns | 87      |
end
| Figure9.3 | Classicexampleofinheritance:colorpoint |     |     |     |
| --------- | -------------------------------------- | --- | --- | --- |
thedesignwehavechosenforourlanguage;otherobject-orientedlanguages
mayhavedifferentvisibilityrules.
Wenextconsiderwhathappenswhenthefieldsormethodsofaclassare
redeclared.Ifafieldofc isredeclaredinoneofitssubclassesc ,thenewdec-
|     | 1   |     |     | 2   |
| --- | --- | --- | --- | --- |
larationshadowstheoldone,justasinlexicalscoping. Forexample,consider
figure9.4. Anobjectofclassc2hastwofieldsnamedy: theonedeclaredin
c1andtheonedeclaredinc2. Themethodsdeclaredinc1seec1’sfieldsx
andy. Inc2,thex ingetx2referstoc1’sfieldx,buttheyingety2refers
toc2’sfieldy.

9.2 Inheritance 331
| class | c1                | extends object |       |     |
| ----- | ----------------- | -------------- | ----- | --- |
|       | field x           |                |       |     |
|       | field y           |                |       |     |
|       | method initialize | ()             | 1     |     |
|       | method setx1      | (v) set        | x = v |     |
|       | method sety1      | (v) set        | y = v |     |
|       | method getx1      | () x           |       |     |
|       | method gety1      | () y           |       |     |
| class | c2                | extends c1     |       |     |
|       | field y           |                |       |     |
|       | method sety2      | (v) set        | y = v |     |
|       | method getx2      | () x           |       |     |
|       | method gety2      | () y           |       |     |
| let   | o2 =              | new c2()       |       |     |
in begin
|     | send      | o2 setx1(101);   |           |     |
| --- | --------- | ---------------- | --------- | --- |
|     | send      | o2 sety1(102);   |           |     |
|     | send      | o2 sety2(999);   |           |     |
|     | list(send | o2 getx1(),      | % returns | 101 |
|     |           | send o2 gety1(), | % returns | 102 |
|     |           | send o2 getx2(), | % returns | 101 |
|     |           | send o2 gety2()) | % returns | 999 |
end
|     |     | Figure9.4 Exampleoffieldshadowing |     |     |
| --- | --- | --------------------------------- | --- | --- |
If a method m of a class c 1 is redeclared in one of its subclasses c 2 , we
say that the new method overrides the old one. We call the class in which
a method is declaredthat method’s host class. Similarly, we define the host
classofanexpressiontobethehostclassofthemethod(ifany)inwhichthe
expressionoccurs. We alsodefine the superclassof amethod or expression
astheparentclassofitshostclass.
| Ifanobjectofclass |     | c issentanm | message,thenthenewmethodshould |     |
| ----------------- | --- | ----------- | ------------------------------ | --- |
2
be used. This rule is simple, but it has subtle consequences. Consider the
followingexample:

| 332 |              |            |        |            |            | 9 ObjectsandClasses |
| --- | ------------ | ---------- | ------ | ---------- | ---------- | ------------------- |
|     | class c1     | extends    | object |            |            |                     |
|     | method       | initialize | ()     | 1          |            |                     |
|     | method       | m1 () 11   |        |            |            |                     |
|     | method       | m2 () send | self   | m1()       |            |                     |
|     | class c2     | extends    | c1     |            |            |                     |
|     | method       | m1 () 22   |        |            |            |                     |
|     | let o1 =     | new c1()   | o2     | = new c2() |            |                     |
|     | in list(send | o1         | m1(),  | send o2    | m1(), send | o2 m2())            |
We expectsend o1 m1()to return11, since o1 isaninstance of c1. Sim-
ilarly,weexpectsend o2 m1()toreturn22,sinceo2isaninstanceofc2.
|     |            | send | o2 m2()? |        | m2          |              |
| --- | ---------- | ---- | -------- | ------ | ----------- | ------------ |
| Now | what about |      |          | Method | immediately | calls method |
m1,butwhichone?
Dynamic dispatch tells us that we should look at the class of the object
boundtoself.Thevalueofselfiso2,whichisofclassc2. Hencethecall
| send | self m1()shouldreturn22. |     |     |     |     |     |
| ---- | ------------------------ | --- | --- | --- | --- | --- |
Our language has one more important feature, super calls. Consider the
programinfigure9.5. Therewe havesuppliedtheclass colorpointwith
anoverlyspecializedinitializemethodthatsetsthefieldcoloraswell
asthe fieldsxandy. However, thebodyofthenewmethodduplicatesthe
codeoftheoverriddenone. Thismightbeacceptableinoursmallexample,
butin a largeexamplethis would clearlybe bad practice. (Why?) Further-
more,ifcolorpointdeclaredafieldx,therewouldbenowaytoinitialize
x point, y
the field of just as there is no way to initialize the first in the
exampleonpage331.
Thesolutionistoreplacetheduplicatedcodeinthebodyofcolorpoint’s
initializemethod with a super call of the form super initialize().
Thentheinitializemethodincolorpointwouldread
|     | method initialize |     | (initx, | inity, | initcolor) |     |
| --- | ----------------- | --- | ------- | ------ | ---------- | --- |
begin
|     | super     | initialize(initx, |     | inity); |     |     |
| --- | --------- | ----------------- | --- | ------- | --- | --- |
|     | set color | = initcolor       |     |         |     |     |
end
| Asupercallsupern(...) |     |     | inthebodyofamethodminvokesamethod |     |     |     |
| --------------------- | --- | --- | --------------------------------- | --- | --- | --- |
noftheparentofm’shostclass.Thisisnotnecessarilytheparentoftheclass
ofself. Theclassofselfwillalwaysbeasubclassofm’shostclass,butit
maynotbethesame,becausemmighthavebeendeclaredinanancestorof
thetargetobject.

| 9.2 Inheritance |               |                |     |     | 333 |
| --------------- | ------------- | -------------- | --- | --- | --- |
| class           | point extends | object         |     |     |     |
| field           | x             |                |     |     |     |
| field           | y             |                |     |     |     |
| method          | initialize    | (initx, inity) |     |     |     |
begin
set x = initx;
set y = inity
end
| method | move (dx, | dy) |     |     |     |
| ------ | --------- | --- | --- | --- | --- |
begin
set x = +(x,dx);
set y = +(y,dy)
end
| method | get-location | () list(x,y)   |            |     |     |
| ------ | ------------ | -------------- | ---------- | --- | --- |
| class  | colorpoint   | extends point  |            |     |     |
| field  | color        |                |            |     |     |
| method | initialize   | (initx, inity, | initcolor) |     |     |
begin
set x = initx;
set y = inity;
set color = initcolor
end
| method  | set-color                 | (c) set color | = c |     |     |
| ------- | ------------------------- | ------------- | --- | --- | --- |
| method  | get-color                 | () color      |     |     |     |
| let o1  | = new colorpoint(3,4,172) |               |     |     |     |
| in send | o1 get-color()            |               |     |     |     |
Figure9.5 Exampledemonstratinganeedforsuper
To illustrate this distinction, consider figure 9.6. Sending an m3 mes-
|                   | o3       | c3 c2’s |        | m3,       |          |
| ----------------- | -------- | ------- | ------ | --------- | -------- |
| sage to an object | of class | finds   | method | for which | executes |
super m1(). Theclassofo3isc3,whoseparentisc2. Butthehostclassis
c2,andc2’ssuperclassisc1. Soc1’smethodform1isexecuted. Thisisan
exampleofstaticmethoddispatch.Thoughtheobjectofasupermethodcallis
self, method dispatchis static, becausethe specific method tobe invoked
canbedeterminedfromthetext,independentoftheclassofself.
Inthisexample,c1’smethodform1callso3’sm2method. Thisisanordi-
narymethodcall, sodynamicdispatchisused, soitisc3’sm2methodthat
isfound,returning33.

334 9 ObjectsandClasses
| class c1  | extends object                                    |      |     |
| --------- | ------------------------------------------------- | ---- | --- |
| method    | initialize ()                                     | 1    |     |
| method    | m1 () send self                                   | m2() |     |
| method    | m2 () 13                                          |      |     |
| class c2  | extends c1                                        |      |     |
| method    | m1 () 22                                          |      |     |
| method    | m2 () 23                                          |      |     |
| method    | m3 () super                                       | m1() |     |
| class c3  | extends c2                                        |      |     |
| method    | m1 () 32                                          |      |     |
| method    | m2 () 33                                          |      |     |
| let o3 =  | new c3()                                          |      |     |
| in send   | o3 m3()                                           |      |     |
| Figure9.6 | Exampleillustratinginteractionofsupercallwithself |      |     |
9.3 The Language
For our language CLASSES, we extend the language IMPLICIT-REFS with
theadditionalproductionsshowninfigure9.7. Aprogramisasequenceof
class declarationsfollowed by an expression to be executed. A class decla-
rationhasaname,animmediatesuperclassname,zeroormorefielddecla-
rations,andzeroormoremethoddeclarations. Amethoddeclaration,likea
proceduredeclarationinaletrec,hasaname,alistofformalparameters,
and a body. We also extend the language with multiargument procedures,
multideclarationlet,letrecexpressions,andsomeadditionaloperations
likeadditionandlist.Theoperationsonlistsareasinexercise3.9.Last,we
addabeginexpression,asinexercise4.4,thatevaluatesitssubexpressions
fromlefttorightandreturnsthevalueofthelastone.
Weaddobjectsandlistsasexpressedvalues,sowehave
|        | = +           | + +                      | +   |
| ------ | ------------- | ------------------------ | --- |
| ExpVal | Int           | Bool Proc Listof(ExpVal) | Obj |
| DenVal | = Ref(ExpVal) |                          |     |
We write Listof(ExpVal) to indicate that the lists may contain any expressed
value.
We will consider Obj in section 9.4.1. Classes are neither denotable nor
expressible in our language: they may appear as part of objects but nev-
er as the binding of a variable or the value of an expression, but see exer-
cise9.29.

9.3 TheLanguage 335
| Program   | ::={ClassDecl}∗ |            | Expression   |               |            |     |     |
| --------- | --------------- | ---------- | ------------ | ------------- | ---------- | --- | --- |
|           | a-program       |            | (class-decls |               | body)      |     |     |
| ClassDecl | ::= class       | Identifier |              | extends       | Identifier |     |     |
|           | {field          |            | Identifier}∗ | {MethodDecl}∗ |            |     |     |
a-class-decl
|                     |                 | (class-name |                    | super-name         |                    |             |        |
| ------------------- | --------------- | ----------- | ------------------ | ------------------ | ------------------ | ----------- | ------ |
|                     |                 | field-names |                    | method-decls)      |                    |             |        |
| MethodDecl::=method |                 |             | Identifier         | ({Identifier}∗(,)) |                    | Expression  |        |
|                     | a-method-decl   |             |                    | (method-name       |                    | vars body)  |        |
| Expression          | ::=new          | Identifier  | ({Expression}∗(,)) |                    |                    |             |        |
|                     | new-object-exp  |             |                    | (class-name        |                    | rands)      |        |
| Expression          | ::=send         | Expression  |                    | Identifier         | ({Expression}∗(,)) |             |        |
|                     | method-call-exp |             |                    | (obj-exp           |                    | method-name | rands) |
| Expression          | ::=super        | Identifier  |                    | ({Expression}∗(,)) |                    |             |        |
|                     | super-call-exp  |             |                    | (method-name       |                    | rands)      |        |
| Expression          | ::=self         |             |                    |                    |                    |             |        |
|                     | self-exp        |             | ()                 |                    |                    |             |        |
Figure9.7 Newproductionsforasimpleobject-orientedprogramminglanguage
Wehaveincludedfouradditionalexpressions. Thenewexpressioncreates
an object of the named class. The initializemethod is then invoked to
initialize the fields of the object. The rands are evaluated and passed to
theinitializemethod. Thevaluereturnedbythismethodcallisthrown
awayandthenewobjectisreturnedasthevalueofthenewexpression.
Aselfexpressionreturnstheobjectonwhichthecurrentmethodisoper-
ating.
A send expression consists of an expression that should evaluate to an
object, a method name, and zero or more operands. The named method
is retrieved from the class of the object, and then is passed the arguments
obtainedbyevaluatingtheoperands. AsinIMPLICIT-REFS,anewlocation
isallocatedforeachofthesearguments,andthenthemethodbodyisevalu-

336 9 ObjectsandClasses
atedwithinthescopeoflexicalbindingsassociatingthemethod’sparameters
withthereferencestothecorrespondinglocations.
A super-call expression consists of a method name and zero or more
arguments.Itlooksforamethodofthegivenname,startinginthesuperclass
oftheexpression’shostclass.Thebodyofthemethodisthenevaluated,with
thecurrentobjectasself.
9.4 The Interpreter
Whenaprogramisevaluated,alltheclassdeclarationsareprocessedusing
initialize-class-env!andthentheexpressionisevaluated. Thepro-
cedure initialize-class-env! creates a global class environment that
mapseachclassnametothemethodsoftheclass. Becausethisenvironment
isglobal,wemodelitasaSchemevariable.Wediscusstheclassenvironment
inmoredetailinsection9.4.3.
→
| value-of-program         | : Program | ExpVal |
| ------------------------ | --------- | ------ |
| (define value-of-program |           |        |
| (lambda (pgm)            |           |        |
(initialize-store!)
| (cases                 | program pgm  |                 |
| ---------------------- | ------------ | --------------- |
| (a-program             | (class-decls | body)           |
| (initialize-class-env! |              | class-decls)    |
| (value-of              | body         | (init-env)))))) |
The procedure value-of contains, as usual, a clause for each kind of
expressioninthelanguage,includingaclauseforeachofthefournewpro-
ductions.
Weconsidereachnewkindofexpressioninturn.
Usually, an expression is evaluated because it is part of a method that is
operating on some object. In the environment, this object is bound to the
pseudo-variable %self. We call this a pseudo-variable because it is bound
lexically,likeanordinaryvariable,butithassomewhatdifferentproperties,
which we explore below. Similarly, the name of the superclass of the host
classofthecurrentmethodisboundtothepseudo-variable%super.
Whenaselfexpressionisevaluated,thevalueof%selfisreturned.The
clauseinvalue-ofis
| (self-exp  | ()  |          |
| ---------- | --- | -------- |
| (apply-env | env | ’%self)) |

9.4 TheInterpreter 337
Whenasendexpressionisevaluated,theoperandsandtheobjectexpres-
sionareevaluated.Welookintheobjecttofinditsclassname. Thenwefind
the method using find-method, which takes a class name and a method
name and returns a method. That method is then applied to the current
objectandthemethodarguments.
| (method-call-exp |        | (obj-exp          | method-name | rands) |
| ---------------- | ------ | ----------------- | ----------- | ------ |
| (let             | ((args | (values-of-exps   | rands       | env))  |
|                  | (obj   | (value-of obj-exp | env)))      |        |
(apply-method
(find-method
|     | (object->class-name |     | obj) |     |
| --- | ------------------- | --- | ---- | --- |
method-name)
obj
args)))
Supermethodinvocationissimilartoordinarymethodinvocationexcept
thatthemethodislookedupinthesuperclassofthehostclassoftheexpres-
sion. Theclauseinvalue-ofis
| (super-call-exp |        | (method-name    | rands)    |       |
| --------------- | ------ | --------------- | --------- | ----- |
| (let            | ((args | (values-of-exps | rands     | env)) |
|                 | (obj   | (apply-env env  | ’%self))) |       |
(apply-method
|     | (find-method | (apply-env | env ’%super) | method-name) |
| --- | ------------ | ---------- | ------------ | ------------ |
obj
args)))
Our last task is to create objects. When a new expression is evaluated,
theoperandsareevaluatedandanewobjectiscreatedfromtheclassname.
Thenitsinitializemethodiscalled,butitsvalueisignored.Finally,theobject
isreturned.
| (new-object-exp |        | (class-name     | rands)        |       |
| --------------- | ------ | --------------- | ------------- | ----- |
| (let            | ((args | (values-of-exps | rands         | env)) |
|                 | (obj   | (new-object     | class-name))) |       |
(apply-method
|     | (find-method | class-name | ’initialize) |     |
| --- | ------------ | ---------- | ------------ | --- |
obj
args)
obj))
Next we determine how to represent objects, methods, and classes. To
illustratetherepresentation,weusearunningexample,showninfigure9.8.

338 9 ObjectsandClasses
| class c1 | extends object |     |     |
| -------- | -------------- | --- | --- |
field x
field y
| method | initialize | ()  |     |
| ------ | ---------- | --- | --- |
begin
| set x | = 11; |     |     |
| ----- | ----- | --- | --- |
| set y | = 12  |     |     |
end
| method   | m1 () ...  | x ... y ...    |     |
| -------- | ---------- | -------------- | --- |
| method   | m2 () ...  | send self m3() | ... |
| class c2 | extends c1 |                |     |
field y
| method | initialize | ()  |     |
| ------ | ---------- | --- | --- |
begin
| super | initialize(); |     |     |
| ----- | ------------- | --- | --- |
| set y | = 22          |     |     |
end
| method   | m1 (u,v) ... | x ... y ... |     |
| -------- | ------------ | ----------- | --- |
| method   | m3 () ...    |             |     |
| class c3 | extends c2   |             |     |
field x
field z
| method | initialize | ()  |     |
| ------ | ---------- | --- | --- |
begin
| super | initialize(); |     |     |
| ----- | ------------- | --- | --- |
| set x | = 31;         |     |     |
| set z | = 32          |     |     |
end
| method    | m3 () ...                         | x ... y ... | z ... |
| --------- | --------------------------------- | ----------- | ----- |
| let o3 =  | new c3()                          |             |       |
| in send   | o3 m1(7,8)                        |             |       |
| Figure9.8 | SampleprogramforOOPimplementation |             |       |

9.4 TheInterpreter 339
an-object c3
Figure9.9 Asimpleobject
9.4.1 Objects
Werepresentanobjectasadatatypecontainingtheobject’sclassnameand
alistofreferencestoitsfields.
(define-datatype object object?
(an-object
(class-name identifier?)
(fields (list-of reference?))))
We lay out the list with the fields from the “oldest” class first. Thus in
figure 9.8, an object of class c1 would have its fields laid out as (x y); an
object of class c2 would lay out its fields as (x y y), with the second y
beingtheonebelongingtoc2,andanobjectofclassc3wouldbelaidoutas
(x y y x z). The representationof object o3 fromfigure9.8isshown in
figure9.9. Ofcourse,wewantthemethodsinclassc3torefertothefieldx
declaredinc3,nottheonedeclaredinc1. Wetakecareofthiswhenweset
uptheenvironmentforevaluationofthemethodbody.
This strategy has the useful property that any subclass of c3 will have
these fields in the same positions in the list, because any fields added later
willappeartotherightofthesefields. Whatisthepositionofxinamethod
thatisdefinedinanysubclassofc3? Assuming thatxisnotredefined,we
know that the position of x must be 3 throughout all such methods. Thus,
when a field variable is declared, the position of the corresponding value
remainsunchanged. Thispropertyallowsfieldreferencestobedetermined
statically,similarlytothewaywehandledvariablesinsection3.6.
Makinganewobjectiseasy. We simplycreateanan-objectwith alist
of new referencesequal to the number of fields in the object. To determine
that number, we get the list of field variables from the object’s class. We
initializeeachlocationwithanillegalvaluethatwillberecognizableincase
theprogramdereferencesthelocationwithoutinitializingit.

340 9 ObjectsandClasses
| ClassName  | = Sym        |       |     |
| ---------- | ------------ | ----- | --- |
| new-object | : ClassName  | → Obj |     |
| (define    | new-object   |       |     |
| (lambda    | (class-name) |       |     |
(an-object
class-name
(map
(lambda (field-name)
|     | (newref             | (list ’uninitialized-field | field-name)))    |
| --- | ------------------- | -------------------------- | ---------------- |
|     | (class->field-names | (lookup-class              | class-name)))))) |
9.4.2 Methods
Wenextturntomethods. Methodsarelikeprocedures,exceptthattheydo
nothaveasavedenvironment. Instead,theykeeptrackofthenamesofthe
fieldstowhich theyrefer. Whenamethod is applied,itrunsitsbodyinan
environmentinwhich
• Themethod’sformalparametersareboundtonewreferencesthatareini-
tializedtothevaluesofthearguments. Thisisanalogoustothebehavior
ofapply-procedureinIMPLICIT-REFS.
• Thepseudo-variables%selfand%superareboundtothecurrentobject
andthemethod’ssuperclass,respectively.
• The visible field names are bound to the fields of the current object. To
implementthis,wedefine
| (define-datatype |     | method method? |     |
| ---------------- | --- | -------------- | --- |
(a-method
|     | (vars (list-of     | identifier?))            |     |
| --- | ------------------ | ------------------------ | --- |
|     | (body expression?) |                          |     |
|     | (super-name        | identifier?)             |     |
|     | (field-names       | (list-of identifier?)))) |     |

| 9.4 TheInterpreter |              |              |       |                    |        |              |        | 341 |
| ------------------ | ------------ | ------------ | ----- | ------------------ | ------ | ------------ | ------ | --- |
|                    |              |              | ×     | ×                  |        |              | →      |     |
| apply-method       |              | : Method     |       | Obj Listof(ExpVal) |        |              | ExpVal |     |
| (define            | apply-method |              |       |                    |        |              |        |     |
| (lambda            |              | (m self      | args) |                    |        |              |        |     |
|                    | (cases       | method       | m     |                    |        |              |        |     |
|                    | (a-method    | (vars        | body  | super-name         |        | field-names) |        |     |
|                    | (value-of    |              | body  |                    |        |              |        |     |
|                    |              | (extend-env* |       | vars (map          | newref |              | args)  |     |
(extend-env-with-self-and-super
self super-name
|     |     | (extend-env |     | field-names |     | (object->fields |     | self) |
| --- | --- | ----------- | --- | ----------- | --- | --------------- | --- | ----- |
(empty-env)))))))))
extend-env*
| Here we | use |     | from | exercise | 2.10, | which | extends | an envi- |
| ------- | --- | --- | ---- | -------- | ----- | ----- | ------- | -------- |
ronment by binding a list of variables to a list of denoted values. We have
alsoaddedtoourenvironmentinterfacetheprocedureextend-env-with-
self-and-super,whichbinds%selfand%supertoanobjectandaclass
name,respectively.
In order to make sure that each method sees the right fields, we need to
be careful when constructing the field-names list. Each method should
seeonlythelastdeclarationofafield;alltheothersshouldbeshadowed. So
whenweconstructthe field-nameslist, wewillreplaceallbuttheright-
most occurrence of each name with a fresh identifier. For the program of
figure9.8,theresultingfield-namesfieldslooklike
| Class | FieldsDefined |     | Fields |       |     | field-names |            |     |
| ----- | ------------- | --- | ------ | ----- | --- | ----------- | ---------- | --- |
| c1    | x,            | y   | (x     | y)    |     | (x          | y)         |     |
| c2    | y             |     | (x     | y y)  |     | (x          | y%1 y)     |     |
| c3    | x,            | z   | (x     | y y x | z)  | (x%1        | y%1 y x z) |     |
Sincethemethodbodiesdonotknowanythingaboutx%1ory%1,theycan
onlyseetherightmostfieldforeachfieldvariable,asdesired.
Figure9.10showstheenvironmentbuiltfortheevaluationofthemethod
|        | send | o3 m1(7,8)infigure |     |      |      |        |               |        |
| ------ | ---- | ------------------ | --- | ---- | ---- | ------ | ------------- | ------ |
| bodyin |      |                    |     | 9.8. | This | figure | shows thatthe | listof |
referencesmaybelongerthanthelistofvariables: thelistofvariablesisjust
(x y%1 y),sincethosearetheonlyfieldvariablesvisiblefrommethodm1
inc2,butthevalueof(object->fields self)isthelistofallthefields
of the object. However, since the values of the three visible field variables
areinthefirstthreeelementsofthelist,andsincewehaverenamedthefirst
y to be y%1 (which the method knows nothing about) the method m1 will
associatethevariableywiththeydeclaredinc2,asdesired.

342 9 ObjectsandClasses
(u v)
( )
( %self %super)
( an-object c3 c1 )
(x y%1 y)
Figure9.10 Environmentformethodapplication
When the host class and the class of self are the same, the list of vari-
ablesisgenerallyofthesamelengthasthelistoffieldlocations. Ifthehost
classishigheruptheclasschain,thentheremaybemorelocationsthanfield
variables, but the values corresponding to the field variables will be at the
beginningofthelist,andtheextravalueswillbeinaccessible.
9.4.3 ClassesandClassEnvironments
Our implementation so far has depended on the ability to get information
about a class from its name. So we need a class environment to accomplish
this task. The classenvironment will associate eachclassname with a data
structurethatdescribestheclass.
The class environment is global: in our language, class declarations are
groupedatthebeginningoftheprogramandareinforcefortheentirepro-
gram. So, we represent the class environment as a global variable named
the-class-env,whichwillcontainalistof(class-name,class)lists,butwe
hide this representationbehind the proceduresadd-to-class-env! and
lookup-class.

9.4 TheInterpreter 343
Listof(List(ClassName,Class))
ClassEnv =
| the-class-env     | : ClassEnv        |        |             |
| ----------------- | ----------------- | ------ | ----------- |
| (define           | the-class-env     | ’())   |             |
|                   | :                 | ×      | →           |
| add-to-class-env! | ClassName         | Class  | Unspecified |
| (define           | add-to-class-env! |        |             |
| (lambda           | (class-name       | class) |             |
| (set!             | the-class-env     |        |             |
(cons
|     | (list class-name | class) |     |
| --- | ---------------- | ------ | --- |
the-class-env))))
→
| lookup-class | : ClassName           | Class             |                  |
| ------------ | --------------------- | ----------------- | ---------------- |
| (define      | lookup-class          |                   |                  |
| (lambda      | (name)                |                   |                  |
| (let         | ((maybe-pair          | (assq name        | the-class-env))) |
| (if          | maybe-pair            | (cadr maybe-pair) |                  |
|              | (report-unknown-class |                   | name)))))        |
Foreachclass,weneedtokeeptrackofthreethings: thenameofitssuper-
class,thelistofitsfieldvariables,andanenvironmentmappingitsmethod
namestoitsmethods.
| (define-datatype | class | class? |     |
| ---------------- | ----- | ------ | --- |
(a-class
| (super-name  | (maybe                 | identifier?)) |     |
| ------------ | ---------------------- | ------------- | --- |
| (field-names | (list-of               | identifier?)) |     |
| (method-env  | method-environment?))) |               |     |
Here we use the predicate (maybe identifier?) which is satisfied by
anyvaluethatiseitherasymboloris#f. Thelatterpossibilityisnecessary
for the class object, which has no superclass. The field-names will be
thefieldsoftheclass,asseenbymethodsofthatclass,andmethodswillbe
anenvironmentgivingadefinitiontoeachmethodnamethatisdefinedfor
theclass.
Wewillinitializetheclassenvironmentwithanentryfortheclassobject.
Foreachdeclaration,weaddanewbindingtotheclassenvironment,bind-
ing the name of the class to a class consisting of the name of the super-
class,thefield-namesforthemethodsofthatclass,andtheenvironment
ofmethodsforthatclass.

344 9 ObjectsandClasses
→
| initialize-class-env! |                       |     | : Listof(ClassDecl) |     | Unspecified |     |
| --------------------- | --------------------- | --- | ------------------- | --- | ----------- | --- |
| (define               | initialize-class-env! |     |                     |     |             |     |
| (lambda               | (c-decls)             |     |                     |     |             |     |
|                       | (set! the-class-env   |     |                     |     |             |     |
(list
|                        | (list                  | ’object                | (a-class    |               | #f ’() ’()))))   |     |
| ---------------------- | ---------------------- | ---------------------- | ----------- | ------------- | ---------------- | --- |
|                        | (for-each              | initialize-class-decl! |             |               | c-decls)))       |     |
| initialize-class-decl! |                        |                        | : ClassDecl | → Unspecified |                  |     |
| (define                | initialize-class-decl! |                        |             |               |                  |     |
| (lambda                | (c-decl)               |                        |             |               |                  |     |
|                        | (cases                 | class-decl             | c-decl      |               |                  |     |
|                        | (a-class-decl          |                        | (c-name     | s-name        | f-names m-decls) |     |
(let ((f-names
(append-field-names
|     |     |     | (class->field-names |     | (lookup-class | s-name)) |
| --- | --- | --- | ------------------- | --- | ------------- | -------- |
f-names)))
(add-to-class-env!
c-name
|     |     | (a-class | s-name | f-names |     |     |
| --- | --- | -------- | ------ | ------- | --- | --- |
(merge-method-envs
|     |     |     | (class->method-env |     | (lookup-class | s-name)) |
| --- | --- | --- | ------------------ | --- | ------------- | -------- |
(method-decls->method-env
|     |     |     | m-decls | s-name | f-names))))))))) |     |
| --- | --- | --- | ------- | ------ | ---------------- | --- |
Theprocedureappend-field-namesisusedtocreatethefield-names
for the current class. It appends the fields of the superclass and the fields
declaredbythenewclass,exceptthatanyfieldofthesuperclassthatisshad-
owed by a new field is replaced by a fresh identifier, as in the example on
page341.
:
append-field-names
|                   |                    |     | ×                 |             | →                 |     |
| ----------------- | ------------------ | --- | ----------------- | ----------- | ----------------- | --- |
| Listof(FieldName) |                    |     | Listof(FieldName) |             | Listof(FieldName) |     |
| (define           | append-field-names |     |                   |             |                   |     |
| (lambda           | (super-fields      |     |                   | new-fields) |                   |     |
(cond
|     | ((null? | super-fields) |     | new-fields) |     |     |
| --- | ------- | ------------- | --- | ----------- | --- | --- |
(else
(cons
|     | (if | (memq             | (car           | super-fields) | new-fields)    |     |
| --- | --- | ----------------- | -------------- | ------------- | -------------- | --- |
|     |     | (fresh-identifier |                | (car          | super-fields)) |     |
|     |     | (car              | super-fields)) |               |                |     |
(append-field-names
|     |     | (cdr | super-fields) | new-fields)))))) |     |     |
| --- | --- | ---- | ------------- | ---------------- | --- | --- |

9.4 TheInterpreter 345
9.4.4 MethodEnvironments
Allthat’slefttodoistowritefind-methodandmerge-method-envs.
As we did for classes, we represent a method environment by a list of
(method-name,method)lists. Welookupamethodusingfind-method.
MethodEnv = Listof(List(MethodName,Method))
find-method : Sym × Sym → Method
(define find-method
(lambda (c-name name)
(let ((m-env (class->method-env (lookup-class c-name))))
(let ((maybe-pair (assq name m-env)))
(if (pair? maybe-pair) (cadr maybe-pair)
(report-method-not-found name))))))
With this information we can write method-decls->method-env. It
takesthemethoddeclarationsofaclassandcreatesamethodenvironment,
recording for each method its bound variables, its body, the name of the
superclassofthehostclass,andthefield-namesofthehostclass.
method-decls->method-env :
Listof(MethodDecl) × ClassName × Listof(FieldName) → MethodEnv
(define method-decls->method-env
(lambda (m-decls super-name field-names)
(map
(lambda (m-decl)
(cases method-decl m-decl
(a-method-decl (method-name vars body)
(list method-name
(a-method vars body super-name field-names)))))
m-decls)))
Last, we write merge-method-envs. Since methods in the new class
override those of the old class, we can simply append the environments,
withthenewmethodsfirst.
merge-method-envs : MethodEnv × MethodEnv → MethodEnv
(define merge-method-envs
(lambda (super-m-env new-m-env)
(append new-m-env super-m-env)))
Therearewaysofbuildingmethodenvironmentsthatwillbemoreefficient
formethodlookup(exercise9.18).

| 346 |     |     |     |     |     | 9 ObjectsandClasses |     |
| --- | --- | --- | --- | --- | --- | ------------------- | --- |
((c3
| #(struct:a-class |                          | c2 (x%2            | y%1 y | x z)               |             |          |          |
| ---------------- | ------------------------ | ------------------ | ----- | ------------------ | ----------- | -------- | -------- |
| ((initialize     |                          | #(struct:a-method  |       | ()                 |             |          |          |
|                  |                          | #(struct:begin-exp |       |                    | ...) c2     | (x%2 y%1 | y x z))) |
| (m3              | #(struct:a-method        |                    | ()    |                    |             |          |          |
|                  | #(struct:diff-exp        |                    | ...)) | c2                 | (x%2 y%1    | y x z))  |          |
| (initialize      |                          | #(struct:a-method  |       | ...))              |             |          |          |
| (m1              | #(struct:a-method        |                    | (u v) |                    |             |          |          |
|                  | #(struct:diff-exp        |                    | ...)  | c1                 | (x y%1 y))) |          |          |
| (m3              | #(struct:a-method        |                    | ...)) |                    |             |          |          |
| (initialize      |                          | #(struct:a-method  |       | ...))              |             |          |          |
| (m1              | #(struct:a-method        |                    | ...)) |                    |             |          |          |
| (m2              | #(struct:a-method        |                    | ()    |                    |             |          |          |
|                  | #(struct:method-call-exp |                    |       | #(struct:self-exp) |             |          | m3 ())   |
|                  | object                   | (x y))))))         |       |                    |             |          |          |
(c2
| #(struct:a-class |                          | c1 (x y%1          | y)    |                    |             |        |        |
| ---------------- | ------------------------ | ------------------ | ----- | ------------------ | ----------- | ------ | ------ |
| ((initialize     |                          | #(struct:a-method  |       | ()                 |             |        |        |
|                  |                          | #(struct:begin-exp |       |                    | ...) c1     | (x y%1 | y)))   |
| (m1              | #(struct:a-method        |                    | (u v) |                    |             |        |        |
|                  | #(struct:diff-exp        |                    | ...)  | c1                 | (x y%1 y))) |        |        |
| (m3              | #(struct:a-method        |                    | ()    |                    |             |        |        |
|                  | #(struct:const-exp       |                    | 23)   | c1                 | (x y%1 y))) |        |        |
| (initialize      |                          | #(struct:a-method  |       | ...))              |             |        |        |
| (m1              | #(struct:a-method        |                    | ...)) |                    |             |        |        |
| (m2              | #(struct:a-method        |                    | ()    |                    |             |        |        |
|                  | #(struct:method-call-exp |                    |       | #(struct:self-exp) |             |        | m3 ()) |
|                  | object                   | (x y))))))         |       |                    |             |        |        |
(c1
| #(struct:a-class |                          | object             | (x y) |                    |             |     |        |
| ---------------- | ------------------------ | ------------------ | ----- | ------------------ | ----------- | --- | ------ |
| ((initialize     |                          | #(struct:a-method  |       | ()                 |             |     |        |
|                  |                          | #(struct:begin-exp |       |                    | ...) object | (x  | y)))   |
| (m1              | #(struct:a-method        |                    | ()    |                    |             |     |        |
|                  | #(struct:diff-exp        |                    | ...)  | object             | (x y)))     |     |        |
| (m2              | #(struct:a-method        |                    | ()    |                    |             |     |        |
|                  | #(struct:method-call-exp |                    |       | #(struct:self-exp) |             |     | m3 ()) |
|                  | object                   | (x y))))))         |       |                    |             |     |        |
(object
| #(struct:a-class |            | #f ()                           | ()))) |     |     |     |     |
| ---------------- | ---------- | ------------------------------- | ----- | --- | --- | --- | --- |
|                  | Figure9.11 | Theclassenvironmentforfigure9.8 |       |     |     |     |     |

9.4 TheInterpreter 347
9.4.5 Exercises
(cid:3)
Exercise9.1 [ ] Implementthefollowingusingthelanguageofthissection:
1. Aqueueclasswithmethodsempty?,enqueue,anddequeue.
2. Extendthequeueclasswithacounterthatcountsthenumberofoperationsthat
havebeenperformedonthecurrentqueue.
3. Extendthequeueclasswithacounterthatcountsthetotalnumberofoperations
thathavebeenperformedonallthequeuesintheclass.Asahint,rememberthat
youcanpassasharedcounterobjectatinitializationtime.
(cid:3)
Exercise9.2 [ ] Inheritance can be dangerous, because a child class can arbitrarily
change the behaviorofamethodbyoverridingit. Defineaclassbogus-oddeven
thatinheritsfromoddevenandoverridesthemethodevensothatlet o1 = new
bogus-oddeven() in send o1 odd (13)givesthewronganswer.
(cid:3)(cid:3)
Exercise9.3 [ ] Infigure9.11,wherearemethodenvironmentsshared?Whereare
thefield-nameslistsshared?
(cid:3)
Exercise9.4 [ ] ChangetherepresentationofobjectssothatanObjcontainstheclass
ofwhichtheobjectisaninstance,ratherthanitsname.Whataretheadvantagesand
disadvantagesofthisrepresentationcomparedtotheoneinthetext?
(cid:3)
Exercise9.5 [ ]Theinterpreterofsection9.4storesthesuperclassnameofamethod’s
hostclassinthelexicalenvironment.Changetheimplementationsothatthemethod
storesthehostclassname,andretrievesthesuperclassnamefromthehostname.
(cid:3)
Exercise9.6 [ ] Add to our language the expression instanceof exp class-name.
Thevalueofthisexpressionshouldbetrueifandonlyiftheobjectobtainedbyeval-
uatingexpisaninstanceofclass-nameorofoneofitssubclasses.
(cid:3)
Exercise9.7 [ ]Inourlanguage,theenvironmentforamethodincludesbindingsfor
thefieldvariablesdeclaredinthehostclassanditssuperclasses. Limitthemtojust
thehostclass.
(cid:3)
Exercise9.8 [ ]Addtoourlanguageanewexpression,
fieldref obj field-name
thatretrievesthecontentsofthegivenfieldoftheobject.Addalso
fieldset obj field-name = exp
whichsetsthegivenfieldtothevalueofexp.
(cid:3)
Exercise9.9 [ ] Addexpressionssuperfieldreffield-nameandsuperfieldset
field-name = exp that manipulate the fields of self that would otherwise be shad-
owed. Remember super is static, and always refers to the superclass of the host
class.

348 9 ObjectsandClasses
(cid:3)(cid:3)
Exercise9.10 [ ]Someobject-orientedlanguagesincludefacilitiesfornamed-class
method invocation and field references. In a named-class method invocation, one
might write named-send c1 o m1(). This would invoke c1’s m1 method on o,
solongasowasaninstanceofc1orofoneofitssubclasses,evenifm1wereover-
ridden in o’s actual class. This is a form of static method dispatch. Named-class
fieldreferenceprovidesasimilarfacilityforfieldreference.Addnamed-classmethod
invocation,fieldreference,andfieldsettingtothelanguageofthissection.
(cid:3)(cid:3)
Exercise9.11 [ ]AddtoCLASSEStheabilitytospecifythateachmethodiseither
private and only accessible fromwithin the host class, protected and only accessible
fromthehostclassanditsdescendants,orpublicandaccessiblefromanywhere.Many
object-orientedlanguagesincludesomeversionofthisfeature.
(cid:3)(cid:3)
Exercise9.12 [ ]AddtoCLASSEStheabilitytospecifythateachfieldiseitherpri-
vate,protected,orpublicasinexercise9.11.
(cid:3)(cid:3)
Exercise9.13 [ ] To defend against malicious subclasses likebogus-oddevenin
exercise9.2,manyobject-orientedlanguageshaveafacilityforfinalmethods,which
maynotbeoverridden.AddsuchafacilitytoCLASSES,sothatwecouldwrite
| class oddeven     | extends object |           |              |
| ----------------- | -------------- | --------- | ------------ |
| method initialize | () 1           |           |              |
| final method      | even (n)       |           |              |
| if zero?(n)       | then 1 else    | send self | odd(-(n,1))  |
| final method      | odd (n)        |           |              |
| if zero?(n)       | then 0 else    | send self | even(-(n,1)) |
(cid:3)(cid:3)
Exercise9.14 [ ]Anotherwaytodefendagainstmalicioussubclassesistousesome
formofstaticdispatch. ModifyCLASSESsothatmethodcallstoself alwaysusethe
methodinthehostclass,ratherthanthemethodintheclassofthetargetobject.
(cid:3)(cid:3)
Exercise9.15 [ ] Many object-oriented languages include a provisionfor static or
classvariables. Staticvariablesassociatesomestatewithaclass;alltheinstancesof
theclasssharethisstate.Forexample,onemightwrite:
| class c1 extends          | object |     |     |
| ------------------------- | ------ | --- | --- |
| static next-serial-number |        | = 1 |     |
field my-serial-number
| method get-serial-number |     | () my-serial-number |     |
| ------------------------ | --- | ------------------- | --- |
| method initialize        | ()  |                     |     |
begin
| set my-serial-number   |     | = next-serial-number;     |     |
| ---------------------- | --- | ------------------------- | --- |
| set next-serial-number |     | = +(next-serial-number,1) |     |
end
| let o1 = new | c1()                    |     |     |
| ------------ | ----------------------- | --- | --- |
| o2 = new     | c1()                    |     |     |
| in list(send | o1 get-serial-number(), |     |     |
| send         | o2 get-serial-number()) |     |     |

9.4 TheInterpreter 349
Eachnewobjectofclassc1receivesanewconsecutiveserialnumber.
Addstaticvariablestoourlanguage. Sincestaticvariablescanappearinamethod
body,apply-methodmustaddadditionalbindingsintheenvironmentitconstructs.
Whatenvironmentshouldbeusedfortheevaluationoftheinitializingexpressionfor
astaticvariable(1intheexampleabove)?
(cid:3)(cid:3)
Exercise9.16 [ ] Object-oriented languages frequently allow overloading of meth-
ods.Thisfeatureallowsaclasstohavemultiplemethodsofthesamename,provided
theyhavedistinctsignatures.Amethod’ssignatureistypicallythemethodnameplus
thetypesofitsparameters. SincewedonothavetypesinCLASSES,wemightover-
loadbasedsimplyonthemethodnameandnumberofparameters. Forexample,a
classmighthavetwoinitializemethods,onewithnoparametersforusewhen
initialization with a defaultfield value is desired,and another with one parameter
forusewhenaparticularfieldvalueisdesired.Extendourinterpretertoallowover-
loadingbasedonthenumberofmethodparameters.
(cid:3)(cid:3)
Exercise9.17 [ ]Asitstands,theclassesinourlanguagearedefinedglobally.Add
toCLASSESafacilityforlocalclasses,soonecanwritesomethinglikeletclass c
= ...in e. Asahint,consideraddingtheclassenvironmentasaparametertothe
interpreter.
(cid:3)(cid:3)
Exercise9.18 [ ] The method environments produced by merge-method-envs
can be long. Write a new versionof merge-method-envswith the propertythat
eachmethodnameoccursexactlyonce,andfurthermore,itappearsinthesameplace
asitsearliestdeclaration.Forexample,infigure9.8,methodm2shouldappearinthe
sameplaceinthemethodenvironmentsofc1,c2,c3,andanydescendantofc3.
(cid:3)(cid:3)
Exercise9.19 [ ]ImplementlexicaladdressingforCLASSES.First,writealexical-
address calculator like that of section 3.7.1 for the language of this section. Then
modify the implementation of environments to make them nameless, and modify
value-ofsothatapply-envtakesalexicaladdressinsteadofasymbol,asinsec-
tion3.7.2.
(cid:3)(cid:3)(cid:3)
Exercise9.20 [ ]Cananythingequivalenttotheoptimizationsoftheexercise9.19
bedoneformethodinvocations?Discusswhyorwhynot.
(cid:3)(cid:3)
Exercise9.21 [ ]Iftherearemanymethodsinaclass,linearsearchdownalistof
methodscanbeslow.Replaceitbysomefasterimplementation.Howmuchimprove-
mentdoesyourimplementationprovide?Accountforyourresults,eitherpositiveor
negative.
(cid:3)(cid:3)
Exercise9.22 [ ]Inexercise9.16,weaddedoverloadingtothelanguagebyextend-
ingtheinterpreter. Anotherwaytosupportoverloadingisnottomodifytheinter-
preter,buttouseasyntacticpreprocessor.Writeapreprocessorthatchangesthename
ofeverymethodmtooneoftheformm:@n,wherenisthenumberofparametersin
the method declaration. It must similarly change the name in every method call,
basedonthenumberofoperands.Weassumethat:@isnotusedbyprogrammersin
methodnames,butisacceptedbytheinterpreterinmethodnames. Compilersfre-
quentlyusesuchatechniquetoimplementmethodoverloading. Thisisaninstance
ofageneraltrickcallednamemangling.

350 9 ObjectsandClasses
(cid:3)(cid:3)(cid:3)
Exercise9.23 [ ]Wehavetreatedsupercallsasiftheywerelexicallybound. But
wecandobetter:wecandeterminesupercallsstatically. Sinceasupercallrefersto
amethodinaclass’sparent,andtheparent,alongwithitsmethods,isknownprior
tothestartofexecution,wecandeterminetheexactmethodtowhichanysupercall
refersatthesametimewedolexical-addressingandotheranalyses.Writeatranslator
thattakeseachsupercallandreplacesitwithanabstractsyntaxtreenodecontaining
theactualmethodtobeinvoked.
(cid:3)(cid:3)(cid:3)
Exercise9.24 [ ]Writeatranslatorthatreplacesmethodnamesinnamedmethod
callsasinexercise9.10withnumbersindicatingtheoffsetofthenamedmethodinthe
run-timemethodtableofthenamedclass.Implementaninterpreterforthetranslated
codeinwhichnamedmethodaccessisconstanttime.
(cid:3)(cid:3)(cid:3)
Exercise9.25 [ ]Usingthefirstexampleofinheritancefromfigure9.5,weinclude
amethodintheclasspointthatdeterminesiftwopointshavethesamex-andy-
coordinates.Weaddthemethodsimilarpointstothepointclassasfollows:
method similarpoints (pt)
if equal?(send pt getx(), x)
then equal?(send pt gety(), y)
else zero?(1)
This works for both kinds of points. Since getx, gety, and similarpointsare
defined in class point, by inheritance, they are defined in colorpoint. Test
similarpoints to compare points with points, points with color points, color
pointswithpoints,andcolorpointswithcolorpoints.
Next consider a small extension. We add a new similarpoints method to the
colorpointclass. Weexpectittoreturntrueifbothpointshavethesamex-and
y-coordinates and further, in case both are color points, they have the same color.
Otherwiseitreturnsfalse.Hereisanincorrectsolution.
method similarpoints (pt)
if super similarpoints(pt)
then equal?(send pt getcolor(),color)
else zero?(1)
Testthisextension.Determinewhyitdoesnotworkonallthecases.Fixitsothatall
thetestsreturnthecorrectvalues.
The difficulty of writing a procedurethat relieson more than one object is known
asthe binarymethodproblem. Itdemonstrates that the class-centricmodelofobject-
orientedprogramming,whichthischapterexplores,leavessomethingtobedesired
whentherearemultipleobjects. Itiscalledthebinarymethodproblembecausethe
problemshowsupwithjusttwoobjects,butitgetsprogressivelyworseasthenumber
ofobjectsincreases.
(cid:3)(cid:3)(cid:3)
Exercise9.26 [ ] Multiple inheritance, inwhich aclass can have morethan one
parent,canbeuseful,butmayintroduceseriouscomplications.Whatiftwoinherited
classesbothhavemethodsofthesamename?Thiscanbedisallowed,orresolvedby

| 9.4 TheInterpreter |     |     |     |     | 351 |
| ------------------ | --- | --- | --- | --- | --- |
enumeratingthemethodsintheclassbysomearbitraryrule,suchasdepth-firstleft-
to-right,orbyrequiringthatthe ambiguityberesolvedatthe pointsuchamethod
iscalled. Thesituationforfieldsisevenworse. Considerthefollowingsituation,in
whichclassc4istoinheritfromc2andc3,bothofwhichinheritfromc1:
| class | c1 extends | object |     |     |     |
| ----- | ---------- | ------ | --- | --- | --- |
field x
| class | c2 extends | c1     |     |     |     |
| ----- | ---------- | ------ | --- | --- | --- |
| class | c3 extends | c1     |     |     |     |
| class | c4 extends | c2, c3 |     |     |     |
Doesaninstanceofc4haveoneinstanceoffieldxsharedbyc2andc3,ordoesc4
havetwoxfields:oneinheritedfromc2andoneinheritedfromc3?Somelanguages
opt for sharing, some not, and some provide a choice, at least in some cases. The
complexityof this problemhas ledto adesigntrend favoringsingle inheritance of
classes,butmultipleinheritanceonlyforinterfaces(section9.5),whichavoidsmost
ofthesedifficulties.
AddmultipleinheritancetoCLASSES.Extendthesyntaxasnecessary.Indicatewhat
issuesarisewhenresolvingmethodandfieldnameconflicts.Characterizethesharing
issueanditsresolution.
(cid:3)(cid:3)(cid:3)
Exercise9.27 [ ]Implementthefollowingdesignforanobjectlanguagewithout
classes. An object will be a set of closures, indexed by method names, that share
anenvironment(andhencesomestate). Classeswillbereplacedbyproceduresthat
returnanobject.Soinsteadofwritingsend o1 m1(11,22,33),wewouldwritean
ordinaryprocedurecall(getmethod(o1,m1) 11 22 33),andinsteadofwriting
| class  | oddeven     | extends object |      |                   |     |
| ------ | ----------- | -------------- | ---- | ----------------- | --- |
| method | initialize  | () 1           |      |                   |     |
| method | even        | (n)            |      |                   |     |
|        | if zero?(n) | then 1 else    | send | self odd(-(n,1))  |     |
| method | odd         | (n)            |      |                   |     |
|        | if zero?(n) | then 0 else    | send | self even(-(n,1)) |     |
| let    | o1 = new    | oddeven()      |      |                   |     |
| in     | send o1     | odd(13)        |      |                   |     |
wemightwritesomethinglike
let make-oddeven
| =   | proc () |     |     |     |     |
| --- | ------- | --- | --- | --- | --- |
newobject
|     | even = | proc (n) if | zero?(n)              | then 1 |         |
| --- | ------ | ----------- | --------------------- | ------ | ------- |
|     |        | else        | (getmethod(self,odd)  |        | -(n,1)) |
|     | odd =  | proc (n) if | zero?(n)              | then 0 |         |
|     |        | else        | (getmethod(self,even) |        | -(n,1)) |
endnewobject
| in  | let o1 = | (make-oddeven) | in  | (getmethod(o1,odd) | 13) |
| --- | -------- | -------------- | --- | ------------------ | --- |

352 9 ObjectsandClasses
(cid:3)(cid:3)(cid:3)
Exercise9.28 [ ]Addinheritancetothelanguageofexercise9.27.
(cid:3)(cid:3)(cid:3)
Exercise9.29 [ ]Design and implement an object-oriented language without
explicit classes, by having each object contain its own method environment. Such
anobjectiscalledaprototype.Replacetheclassobjectbyaprototypeobjectwithno
methodsorfields.Extendaclassbyaddingmethodsandfieldstoitsprototype,yield-
inganewprototype. Thuswemightwritelet c2 = extend c1 ...insteadof
class c2 extends c1 ....Replacethenewoperationwithanoperationclone
thattakesan objectand simplycopiesitsmethodsand fields. Methodsinthis lan-
guageoccurinsidealexicalscope,sotheyshouldhaveaccesstolexicallyvisiblevari-
ables,asusual,aswellasfieldvariables.Whatshadowingrelationshouldholdwhen
afieldvariableofasuperprototypehasthesamenameasavariableinacontaining
lexicalscope?
9.5 ATyped Language
Inchapter7,weshowedhowatypesystemcouldinspectaprogramtoguar-
anteethat it would never executean inappropriateoperation. No program
thatpassesthecheckerwilleverattempttoapplyanonproceduretoanargu-
ment, or to apply a procedure or other operator to the wrong number of
argumentsortoanargumentofthewrongtype.
In this section, we apply this technology to an object-oriented language
that we call TYPED-OO. This language has all the safety properties listed
above, and in addition, no program that passes our checker will ever send
amessagetoanobjectforwhichthereisnocorrespondingmethod,orsend
a message to an object with the wrong number of arguments or with argu-
mentsofthewrongtype.
A sample programin TYPED-OO language is shown in figure 9.12. This
programdefinesaclasstree,whichhasasummethodthatfindsthesumof
thevaluesintheleaves,asinfigure9.2,andanequalmethod,whichtakes
anothertreeandrecursivelydescendsthroughthetreestodetermineifthey
areequal.
Themajornewfeaturesofthelanguageare:
• Fieldsandmethodsarespecifiedwiththeirtypes,usingasyntaxsimilar
tothatusedinchapter7.
• Theconceptofaninterfaceisintroducedinanobject-orientedsetting.
• Theconceptofsubtypepolymorphismisaddedtothelanguage.
• Theconceptofcastingisintroduced,andtheinstanceoftestfromexer-
cise9.6isincorporatedintothelanguage.
Weconsidereachoftheseitemsinturn.

9.5 ATypedLanguage 353
The newproductions for TYPED-OO areshown in figure9.13. We adda
voidtypeasthetypeofasetoperation,andlisttypesasinexercise7.9;as
inexercise7.9werequirethatcallstolisthaveatleastoneargument. We
addidentifierstothesetoftypeexpressions, butforthischapter,anidenti-
fierusedasatypeisassociatedwiththeclassorinterfaceofthesamename.
Weconsiderthiscorrespondenceinmoredetailbelow. Methodsrequiretheir
resulttypetobe specified, alongwiththe typesoftheir arguments, using a
syntax similar to thatused for letrecin chapter 7. Last, two new expres-
sionsareadded,castandinstanceof.
Inordertounderstandthenewfeaturesofthislanguage,wemustdefine
thetypesofthelanguage,aswedidindefinition7.1.1.
Definition9.5.1 Thepropertyofanexpressedvaluevbeingoftypetisdefinedas
follows:
• Ifc isaclass,thenavalueisoftypec ifandonlyifitisanobject,anditisan
instanceoftheclassc oroneofitsdescendants.
• IfIisaninterface,thenavalueisoftypeIifandonlyifitisanobjectthatisan
instanceofaclassthatimplements I. Aclassimplements I ifandonlyifit has
animplementsIdeclarationorifoneofitsancestorsimplements I.
• Iftissomeothertype,thentherulesofdefinition7.1.1apply.
Anobjectisaninstanceofexactlyoneclass,butitcanhavemanytypes.
• Ithasthetypeoftheclassthatcreatedit.
• Ithas the type of thatclass’s superclassand of all classes above it in the
inheritancehierarchy. Inparticular,everyobjecthastypeobject.
• Ithasthetypeofanyinterfacesthatitscreatingclassimplements.
The second property is called subclass polymorphism. The third property
couldbecalledinterfacepolymorphism.
Aninterfacerepresentsthesetofallobjectsthatimplementaparticularset
of methods, regardless of how those objects were constructed. Our typing
system will allow a class c to declare that it implements interface I only if
c provides all the methods, with all the right types, that are required by I.
A class may implement severaldifferentinterfaces, although we have only
usedoneinourexample.

354 9 ObjectsandClasses
interface tree
method int sum ()
method bool equal (t : tree)
class interior-node extends object implements tree
field tree left
field tree right
method void initialize(l : tree, r : tree)
begin
set left = l; set right = r
end
method tree getleft () left
method tree getright () right
method int sum () +(send left sum(), send right sum())
method bool equal (t : tree)
if instanceof t interior-node
then if send left equal(send
cast t interior-node
getleft())
then send right equal(send
cast t interior-node
getright())
else zero?(1)
else zero?(1)
class leaf-node extends object implements tree
field int value
method void initialize (v : int) set value = v
method int sum () value
method int getvalue () value
method bool equal (t : tree)
if instanceof t leaf-node
then zero?(-(value, send cast t leaf-node getvalue()))
else zero?(1)
let o1 = new interior-node (
new interior-node (
new leaf-node(3),
new leaf-node(4)),
new leaf-node(5))
in list(send o1 sum(),
if send o1 equal(o1) then 100 else 200)
Figure9.12 AsampleprograminTYPED-OO

| 9.5 ATypedLanguage |                                     |             |              |              |     | 355 |
| ------------------ | ----------------------------------- | ----------- | ------------ | ------------ | --- | --- |
| ClassDecl          | ::=classIdentifierextendsIdentifier |             |              |              |     |     |
|                    |                                     | {implements |              | Identifier}∗ |     |     |
|                    |                                     | {field      | Identifier}∗ |              |     |     |
Type
{MethodDecl}∗
|           |              | a-class-decl      |            | (c-name               | s-name i-names   |     |
| --------- | ------------ | ----------------- | ---------- | --------------------- | ---------------- | --- |
|           |              |                   |            | f-types               | f-names m-decls) |     |
| ClassDecl | ::=interface |                   | Identifier | {AbstractMethodDecl}∗ |                  |     |
|           |              | an-interface-decl |            | (i-name               | abs-m-decls)     |     |
MethodDecl ::=method Type Identifier ({Identifier : Type}∗(,)) Expression
a-method-decl
|                             |     | (res-type |                 | m-name vars  | var-types   | body) |
| --------------------------- | --- | --------- | --------------- | ------------ | ----------- | ----- |
| AbstractMethodDecl::=method |     |           | Type Identifier | ({Identifier | :Type}∗(,)) |       |
an-abstract-method-decl
|            |               | (result-type              |              | m-name     | m-var-types | m-vars) |
| ---------- | ------------- | ------------------------- | ------------ | ---------- | ----------- | ------- |
| Expression | ::=cast       | Expression                |              | Identifier |             |         |
|            |               | cast-exp                  | (exp         | c-name)    |             |         |
| Expression | ::=instanceof |                           | Expression   | Identifier |             |         |
|            |               | instanceof-exp            |              | (exp name) |             |         |
| Type       | ::=void       |                           |              |            |             |         |
|            |               | void-type                 | ()           |            |             |         |
| Type       | ::=Identifier |                           |              |            |             |         |
|            |               | class-type                | (class-name) |            |             |         |
| Type       | ::=listof     |                           | Type         |            |             |         |
|            |               | list-type                 | (type1)      |            |             |         |
|            | Figure9.13    | NewproductionsforTYPED-OO |              |            |             |         |

356 9 ObjectsandClasses
In figure 9.12, the classes interior-node and leaf-node both imple-
ment the interface tree. The typechecker allows this, because they both
implementthesumandequalmethodsthatarerequiredfortree.
Theexpressioninstanceof e creturnsatruevaluewhenevertheobject
obtainedbyevaluatinge isaninstanceoftheclassc orofoneofitsdescen-
|     | Castingcomplementsinstanceof. |     |     |     | Thevalueofacastexpression |     |     |
| --- | ----------------------------- | --- | --- | --- | ------------------------- | --- | --- |
dants.
cast e c is the same as the value of e if that value is an object that is an
instanceoftheclassc oroneofitsdescendants. Otherwisethecastexpres-
sionreportsanerror. Thetypeofcast e c willalwaysbec,sinceitsvalue,
ifitreturns,isguaranteedtobeoftypec.
Forexample,oursampleprogramincludesthemethod
|     | method | bool       | equal(t         | : tree) |                 |             |     |
| --- | ------ | ---------- | --------------- | ------- | --------------- | ----------- | --- |
|     | if     | instanceof | t interior-node |         |                 |             |     |
|     | then   | if         | send left       |         |                 |             |     |
|     |        |            | equal(send      | cast    | t interior-node | getleft())  |     |
|     |        | then       | send right      |         |                 |             |     |
|     |        |            | equal(send      | cast    | t interior-node | getright()) |     |
|     |        | else       | false           |         |                 |             |     |
|     | else   | false      |                 |         |                 |             |     |
Theexpressioncast t interior-nodecheckstoseeifthevalueoftisan
instanceofinterior-node(oroneofitsdescendants,ifinterior-node
t
had descendants). If it is, the value of is returned; otherwise, an error is
reported. An instanceof expression returns a true value if and only if
the corresponding cast would succeed. Hence in this example the cast is
guaranteed to succeed, since it is guarded by the instanceof. The cast,
in turn, guards the use of send ... getleft(). The cast expression is
guaranteedtoreturnavalueofclassinterior-node,andthereforeitwill
besafetosendthisvalueagetleftmessage.
For our implementation, we begin with the interpreter of section 9.4.1.
| We  | addtwo | new | clausestovalue-ofto |     | evaluateinstanceofand |     | cast |
| --- | ------ | --- | ------------------- | --- | --------------------- | --- | ---- |
expressions:
|     | (cast-exp |      | (exp c-name)    |                     |        |      |         |
| --- | --------- | ---- | --------------- | ------------------- | ------ | ---- | ------- |
|     |           | (let | ((obj (value-of | exp                 | env))) |      |         |
|     |           | (if  | (is-subclass?   | (object->class-name |        | obj) | c-name) |
obj
|     |                 | (report-cast-error |                 |                     | c-name obj)))) |      |         |
| --- | --------------- | ------------------ | --------------- | ------------------- | -------------- | ---- | ------- |
|     | (instanceof-exp |                    |                 | (exp c-name)        |                |      |         |
|     |                 | (let               | ((obj (value-of | exp                 | env)))         |      |         |
|     |                 | (if                | (is-subclass?   | (object->class-name |                | obj) | c-name) |
|     |                 | (bool-val          |                 | #t)                 |                |      |         |
|     |                 | (bool-val          |                 | #f))))              |                |      |         |

| 9.5 ATypedLanguage |     |     |     | 357 |
| ------------------ | --- | --- | --- | --- |
The procedure is-subclass? traces the parent link of the first class
structureuntiliteitherfindsthesecondoneorstopswhentheparentlinkis
#f. Sinceinterfacesareonlyusedastypes,theyareignoredinthisprocess.
| is-subclass? | : ClassName  | × ClassName | → Bool |     |
| ------------ | ------------ | ----------- | ------ | --- |
| (define      | is-subclass? |             |        |     |
| (lambda      | (c-name1     | c-name2)    |        |     |
(cond
| ((eqv? | c-name1 | c-name2) | #t) |     |
| ------ | ------- | -------- | --- | --- |
(else
|     | (let ((s-name | (class->super-name |     |     |
| --- | ------------- | ------------------ | --- | --- |
(lookup-class c-name1))))
|     | (if s-name | (is-subclass? | s-name c-name2) | #f)))))) |
| --- | ---------- | ------------- | --------------- | -------- |
Thiscompletesthemodificationoftheinterpreterforthelanguageofthis
section.
(cid:3)
| Exercise9.30 | [ ] Createaninterfacesummable: |        |     |     |
| ------------ | ------------------------------ | ------ | --- | --- |
| interface    | summable                       |        |     |     |
| method       | int                            | sum () |     |     |
Nowdefineclassesforsummablelists,summablebinarytrees(asinfigure9.12)and
summablegeneraltrees(inwhicheachnodecontainsasummablelistofchildren).
Thendothesamethingforaninterface
| interface | stringable |           |     |     |
| --------- | ---------- | --------- | --- | --- |
| method    | string     | to-string | ()  |     |
Exercise9.31 [ (cid:3) ] Infigure9.12,wouldithaveworkedtomaketreeaclassandhave
thetwonodeclassesinheritfromtree?Inwhatcircumstancesisthisabettermethod
thanusinganinterfacelikesummable?Inwhatcircumstancesisitinferior?
(cid:3)(cid:3)
Exercise9.32 [ ] Write an equalitypredicateforthe class treethat doesnot use
instanceoforcast. Whatisneededhereisadoubledispatch,inplaceofthesingle
dispatchprovidedbytheusualmethods.Thiscanbesimulatedasfollows:Insteadof
usinginstanceoftofindtheclassoftheargumentt,thecurrenttreeshouldsend
backtotamessagethatencodesitsownclass,alongwithparameterscontainingthe
valuesoftheappropriatefields.

358 9 ObjectsandClasses
| 9.6 The | Type Checker |     |     |     |
| ------- | ------------ | --- | --- | --- |
We now turntothe checkerfor this language. The goalof the checkeris to
guarantee a set of safety properties. For our language, these propertiesare
those of the underlying procedurallanguage, plusthe following properties
of the object-oriented portion of the language: no program that passes our
typecheckerwillever
• sendamessagetoanon-object,
• sendamessagetoanobjectforwhichthereisnocorrespondingmethod,
• sendamessagetoanobjectwiththewrongnumberofargumentsorwith
argumentsofthewrongtype.
Wemakenoattempttoverifythattheinitializemethodsactuallyini-
tializeall the fields, so it will still be possible for a programto referencean
uninitialized field. Similarly, because it is in general impossible to predict
thetypeofaninitializemethod,ourcheckerwillnotpreventtheexplicit
invocationofaninitializemethodwiththewrongnumberofarguments
orargumentsofthewrongtype,buttheimplicitinvocationofinitialize
bynewwillalwaysbecorrect.
Thecheckerbeginswiththeimplementationoftype-of-program.Since
allthemethodsofalltheclassesaremutuallyrecursive,weproceedmuchas
| wedoforletrec. |     | Foraletrec,wefirstbuilt |     | tenv-for-letrec-body |
| -------------- | --- | ----------------------- | --- | -------------------- |
bycollectingthedeclaredtypeoftheprocedure(figure7.3).Wethenchecked
eachprocedurebodyagainstitsdeclaredresulttype. Finally,wecheckedthe
bodyoftheletrecintenv-for-letrec-body.
Here, we first call initialize-static-class-env!, which walks
throughtheclassdeclarations,collectingallthetypesintoastaticclassenvi-
ronment. Sincethisenvironmentisglobalandneverchanges, wekeepitin
aSchemevariableratherthanpassingitasaparameter.Thenwecheckeach
classdeclaration, usingcheck-class-decl!. Finally, we find the typeof
thebodyoftheprogram.
| type-of-program |                               | : Program         | → Type           |              |
| --------------- | ----------------------------- | ----------------- | ---------------- | ------------ |
| (define         | type-of-program               |                   |                  |              |
| (lambda         |                               | (pgm)             |                  |              |
|                 | (cases                        | program pgm       |                  |              |
|                 | (a-program                    | (class-decls      | exp1)            |              |
|                 | (initialize-static-class-env! |                   |                  | class-decls) |
|                 | (for-each                     | check-class-decl! |                  | class-decls) |
|                 | (type-of                      | exp1              | (init-tenv)))))) |              |

| 9.6 TheTypeChecker |     |     |     | 359 |
| ------------------ | --- | --- | --- | --- |
Thestaticclassenvironmentwillmapeachclassnametoastaticclasscon-
taining the name of its parent, the names and types of its fields, and the
namesandtypesofitsmethods. Inourlanguage,interfaceshavenoparent
andnofields,sotheywillberepresentedbyadatastructurecontainingonly
thenamesandtypesofitsrequiredmethods(butseeexercise9.36).
| (define-datatype | static-class | static-class? |     |     |
| ---------------- | ------------ | ------------- | --- | --- |
(a-static-class
| (super-name      | (maybe         | identifier?)) |     |     |
| ---------------- | -------------- | ------------- | --- | --- |
| (interface-names | (list-of       | identifier?)) |     |     |
| (field-names     | (list-of       | identifier?)) |     |     |
| (field-types     | (list-of       | type?))       |     |     |
| (method-tenv     | method-tenv?)) |               |     |     |
(an-interface
| (method-tenv | method-tenv?))) |     |     |     |
| ------------ | --------------- | --- | --- | --- |
Beforeconsidering how the static classenvironment isbuilt, we consider
howtoextendtype-oftocheckthetypesofthesixkindsofobject-oriented
expressions:self,instanceof,cast,methodcalls,supercalls,andnew.
For a self expression, we look up the type of self using the pseudo-
variable %self, which we will be sure to bind to the type of the current
hostclass,justasintheinterpreterweboundittothecurrenthostobject.
| instanceof |            |                    | bool.     |     |
| ---------- | ---------- | ------------------ | --------- | --- |
| If an      | expression | returns, it always | returns a | The |
expression cast e c returns the value of e provided that the value is an
objectthatisaninstanceofc oroneofitsdescendants. Hence,ifcast e c
returns a value, then that value is of type c. So we can always assign
cast e cthetypec.Forbothinstanceofandcastexpressions,theinter-
preterevaluatestheargumentandrunsobject->class-nameonit,sowe
wemustofcoursecheckthattheoperandiswell-typedandreturnsavalue
| thatisanobject. Thecodeforthesethreecasesisshowninfigure9.14. |     |     |     |     |
| ------------------------------------------------------------- | --- | --- | --- | --- |
Wenextconsidermethodcalls. Wenowhavethreedifferentkindsofcalls
inourlanguage: procedurecalls,methodcalls,andsupercalls. Weabstract
theprocessofcheckingtheseintoasingleprocedure.

| 360 |                 |              |      |             |     |         | 9 ObjectsandClasses |     |
| --- | --------------- | ------------ | ---- | ----------- | --- | ------- | ------------------- | --- |
|     | (self-exp       | ()           |      |             |     |         |                     |     |
|     | (apply-tenv     |              | tenv | ’%self))    |     |         |                     |     |
|     | (instanceof-exp |              | (exp | class-name) |     |         |                     |     |
|     | (let            | ((obj-type   |      | (type-of    | exp | tenv))) |                     |     |
|     | (if             | (class-type? |      | obj-type)   |     |         |                     |     |
(bool-type)
|              |              | (report-bad-type-to-instanceof                    |              |              |             |          | obj-type      | exp)))) |
| ------------ | ------------ | ------------------------------------------------- | ------------ | ------------ | ----------- | -------- | ------------- | ------- |
|              | (cast-exp    | (exp                                              | class-name)  |              |             |          |               |         |
|              | (let         | ((obj-type                                        |              | (type-of     | exp         | tenv)))  |               |         |
|              | (if          | (class-type?                                      |              | obj-type)    |             |          |               |         |
|              |              | (class-type                                       |              | class-name)  |             |          |               |         |
|              |              | (report-bad-type-to-cast                          |              |              |             | obj-type | exp))))       |         |
|              | Figure9.14   | type-ofclausesforobject-orientedexpressions,part1 |              |              |             |          |               |         |
|              |              | ×                                                 |              | ×            |             | →        |               |         |
| type-of-call | : Type       |                                                   | Listof(Type) |              | Listof(Exp) | Type     |               |         |
| (define      | type-of-call |                                                   |              |              |             |          |               |         |
| (lambda      | (rator-type  |                                                   | rand-types   |              | rands       | exp)     |               |         |
|              | (cases type  | rator-type                                        |              |              |             |          |               |         |
|              | (proc-type   | (arg-types                                        |              | result-type) |             |          |               |         |
|              | (if (not     | (=                                                | (length      | arg-types)   |             | (length  | rand-types))) |         |
(report-wrong-number-of-arguments
|     | (map | type-to-external-form |     |     |     | arg-types)  |     |     |
| --- | ---- | --------------------- | --- | --- | --- | ----------- | --- | --- |
|     | (map | type-to-external-form |     |     |     | rand-types) |     |     |
exp))
|     | (for-each | check-is-subtype! |     |     |     | rand-types | arg-types | rands) |
| --- | --------- | ----------------- | --- | --- | --- | ---------- | --------- | ------ |
result-type)
(else
(report-rator-not-of-proc-type
|     | (type-to-external-form |     |     |     | rator-type) |     |     |     |
| --- | ---------------------- | --- | --- | --- | ----------- | --- | --- | --- |
exp)))))
Thisprocedureisequivalenttothelineforcall-expinCHECKED(fig-
ure7.2)withtwonotableadditions. First,becauseourproceduresnowtake
multiple arguments, we check to see that the call has the right number of
arguments, and in the for-each line we check the type of each operand
againstthetypeofthecorrespondingargumentintheprocedure’stype. Sec-
ond,andmoreinterestingly,wehavereplacedcheck-equal-type!offig-
ure7.2bycheck-is-subtype!.

| 9.6 TheTypeChecker |     |            |     |                         |     |     | 361 |
| ------------------ | --- | ---------- | --- | ----------------------- | --- | --- | --- |
|                    |     | Figure9.15 |     | Subtypingaproceduretype |     |     |     |
Why is this necessary? The principle of subclass polymorphism says
that if class c 2 extends c 1 , then an object of class c 2 can be used in any
context in which an object of class c can appear. If we wrote a proce-
1
dureproc (o : c 1 ) ...,thatprocedureshouldbeabletotakeanactual
| parameteroftypec |     | .   |     |     |     |     |     |
| ---------------- | --- | --- | --- | --- | --- | --- | --- |
2
Ingeneral,wecanextendthenotionofsubclasspolymorphismtosubtype
polymorphism,aswedidwith<: inchapter8. Wesaythatt isasubtypeof
1
t ifandonlyif
2
| • t 1 andt | 2 areclasses,andt |     | 1 isasubclassoft |     | 2 ,or |     |     |
| ---------- | ----------------- | --- | ---------------- | --- | ----- | --- | --- |
• t isaclassandt isaninterface,and t or oneofitssuperclassesimple-
| 1      |     | 2   |     |     | 1   |     |     |
| ------ | --- | --- | --- | --- | --- | --- | --- |
| mentst | ,or |     |     |     |     |     |     |
2
• t 1 and t 2 areproceduretypes, andthe argumenttypesof t 2 aresubtypes
| oftheargumenttypesoft        |           |     | ,andtheresulttypeoft       |       |             | isasubtypeoft  | .       |
| ---------------------------- | --------- | --- | -------------------------- | ----- | ----------- | -------------- | ------- |
|                              |           |     | 1                          |       |             | 1              | 2       |
| Tounderstandthelastrule,lett |           |     |                            | be(c1 | -> d1),lett | be(c2          | -> d2), |
|                              |           |     |                            | 1     |             | 2              |         |
| withc2                       | < c1andd1 | <   | d2.Letfbeaprocedureoftypet |       |             | . Weclaimthatf |         |
1
alsohastypet . Why? Imaginethatwegivefanargumentoftypec2. Since
2
c2 < c1,theargumentisalsoac1.
Thereforeitisanacceptableargument
tof. fthenreturnsavalueoftyped1. Butsinced1 < d2,thisresultisalso
oftyped2.So,iffisgivenanargumentoftypec2,itreturnsavalueoftype
d2.Hencefhastype(c2 -> d2). Wesaythatsubtypingiscovariantinthe
result type and contravariant in the argument type. See figure 9.15. This is
similartothedefinitionof<:-ifaceinsection8.3.2.

362 9 ObjectsandClasses
Thecodeforthisisshowninfigure9.16.Thecodeusesevery2?,anexten-
sionoftheprocedureevery?fromexercise1.24thattakesatwo-argument
predicateandtwolists,andreturns#tifthelistsareofthesamelengthand
correspondingelementssatisfythepredicate,orreturns#fotherwise.
We can now consider each of the three kinds of calls (figure 9.17). For a
methodcall,wefirstfindthetypesofthetargetobjectandoftheoperands,
asusual. Weusefind-method-type,analogoustofind-method,tofind
the type of the method. If the type of the target is not a class or interface,
then type->class-name will report an error. If there is no correspond-
ing method, then find-method-type will report an error. We then call
type-of-calltoverifythatthetypesoftheoperandsarecompatiblewith
thetypesexpectedbythemethod,andtoreturnthetypeoftheresult.
For a new expression, we first retrieve the class information for the class
name. Ifthereisnoclassassociatedwiththename,atypeerrorisreported.
Last, we call type-of-call with the types of the operands to see if the
call to initialize is safe. If these checks succeed, then the execution of
the expressionis safe. Since the newexpression returnsa new object of the
specifiedclass,thetypeoftheresultisthecorrespondingtypeoftheclass.
We have now completed our discussion of checking expressions in
TYPED-OO,sowenowreturntoconstructingthestaticclassenvironment.
To build the static class environment, initialize-static-class-
env!firstsetsthestaticclassenvironmenttoempty,andthenaddsabinding
fortheclassobject. Itthengoesthrougheachclassorinstancedeclaration
andaddsanappropriateentrytothestaticclassenvironment.
initialize-static-class-env! : Listof(ClassDecl) → Unspecified
(define initialize-static-class-env!
(lambda (c-decls)
(empty-the-static-class-env!)
(add-static-class-binding!
’object (a-static-class #f ’() ’() ’() ’()))
(for-each add-class-decl-to-static-class-env! c-decls)))
The procedure add-class-decl-to-static-class-env! (fig. 9.18)
does the bulk of the work of creating the static classes. For each class, we
mustcollectallitsinterfaces,fields,andmethods:
• A class implements any interfaces that its parent implements, plus the
interfacesthatitclaimstoimplement.
• A class has the fields that its parent has, plus its own, except that
its parent’s fields are shadowed by the locally declared fields. So the
field-names are calculated with append-field-names, just as in
initialize-class-env!(page344).

9.6 TheTypeChecker 363
|                   |                   | ×           | ×    | →           |     |
| ----------------- | ----------------- | ----------- | ---- | ----------- | --- |
| check-is-subtype! |                   | : Type Type | Exp  | Unspecified |     |
| (define           | check-is-subtype! |             |      |             |     |
| (lambda           | (ty1              | ty2 exp)    |      |             |     |
| (if               | (is-subtype?      | ty1         | ty2) |             |     |
#t
(report-subtype-failure
|     | (type-to-external-form |     | ty1) |     |     |
| --- | ---------------------- | --- | ---- | --- | --- |
|     | (type-to-external-form |     | ty2) |     |     |
exp))))
| is-subtype? | : Type                   | × Type → | Bool |       |         |
| ----------- | ------------------------ | -------- | ---- | ----- | ------- |
| (define     | is-subtype?              |          |      |       |         |
| (lambda     | (ty1                     | ty2)     |      |       |         |
| (cases      | type                     | ty1      |      |       |         |
|             | (class-type              | (name1)  |      |       |         |
|             | (cases                   | type ty2 |      |       |         |
|             | (class-type              | (name2)  |      |       |         |
|             | (statically-is-subclass? |          |      | name1 | name2)) |
(else #f)))
|     | (proc-type | (args1 res1) |       |     |     |
| --- | ---------- | ------------ | ----- | --- | --- |
|     | (cases     | type ty2     |       |     |     |
|     | (proc-type | (args2       | res2) |     |     |
(and
|     |     | (every2? is-subtype? |      | args2 args1) |     |
| --- | --- | -------------------- | ---- | ------------ | --- |
|     |     | (is-subtype?         | res1 | res2)))      |     |
(else #f)))
|                         | (else (equal?           | ty1 ty2))))) |             |     |      |
| ----------------------- | ----------------------- | ------------ | ----------- | --- | ---- |
| statically-is-subclass? |                         | : ClassName  | × ClassName | →   | Bool |
| (define                 | statically-is-subclass? |              |             |     |      |
| (lambda                 | (name1                  | name2)       |             |     |      |
(or
|     | (eqv? name1       | name2) |     |     |     |
| --- | ----------------- | ------ | --- | --- | --- |
|     | (let ((super-name |        |     |     |     |
(static-class->super-name
|     |     | (lookup-static-class |     | name1)))) |     |
| --- | --- | -------------------- | --- | --------- | --- |
(if super-name
|     | (statically-is-subclass? |     |     | super-name | name2) |
| --- | ------------------------ | --- | --- | ---------- | ------ |
#f))
|     | (let ((interface-names |     |     |     |     |
| --- | ---------------------- | --- | --- | --- | --- |
(static-class->interface-names
|     |       | (lookup-static-class       |                     | name1)))) |     |
| --- | ----- | -------------------------- | ------------------- | --------- | --- |
|     | (memv | name2 interface-names))))) |                     |           |     |
|     |       | Figure9.16                 | SubtypinginTYPED-OO |           |     |

364 9 ObjectsandClasses
| (method-call-exp |             | (obj-exp       |     | method-name | rands)       |
| ---------------- | ----------- | -------------- | --- | ----------- | ------------ |
| (let             | ((arg-types | (types-of-exps |     |             | rands tenv)) |
|                  | (obj-type   | (type-of       |     | obj-exp     | tenv)))      |
(type-of-call
(find-method-type
|     | (type->class-name |     |     | obj-type) |     |
| --- | ----------------- | --- | --- | --------- | --- |
method-name)
arg-types
rands
exp)))
| (super-call-exp |             | (method-name   |     | rands) |              |
| --------------- | ----------- | -------------- | --- | ------ | ------------ |
| (let            | ((arg-types | (types-of-exps |     |        | rands tenv)) |
|                 | (obj-type   | (apply-tenv    |     | tenv   | ’%self)))    |
(type-of-call
(find-method-type
|     | (apply-tenv |     | tenv ’%super) |     |     |
| --- | ----------- | --- | ------------- | --- | --- |
method-name)
arg-types
rands
exp)))
| (new-object-exp |                         | (class-name |                | rands) |               |
| --------------- | ----------------------- | ----------- | -------------- | ------ | ------------- |
| (let            | ((arg-types             |             | (types-of-exps |        | rands tenv))  |
|                 | (c (lookup-static-class |             |                |        | class-name))) |
|                 | (cases static-class     |             | c              |        |               |
|                 | (an-interface           |             | (method-tenv)  |        |               |
(report-cant-instantiate-interface class-name))
|     | (a-static-class |     | (super-name |     | i-names     |
| --- | --------------- | --- | ----------- | --- | ----------- |
|     |                 |     | field-names |     | field-types |
method-tenv)
(type-of-call
(find-method-type
class-name
’initialize)
arg-types
rands
exp)
|            | (class-type                                       |     | class-name))))) |     |     |
| ---------- | ------------------------------------------------- | --- | --------------- | --- | --- |
| Figure9.17 | type-ofclausesforobject-orientedexpressions,part2 |     |                 |     |     |

9.6 TheTypeChecker 365
• Thetypesof the class’sfieldsarethe typesof itsparent’sfields, plusthe
typesofitslocallydeclaredfields.
• The methods of the class are those of its parent plus its own, with their
declaredtypes. We keepthe type of amethod asa proc-type. We put
thelocallydeclaredmethodsfirst,sincetheyoverridetheparent’smeth-
ods.
• Wecheckthattherearenoduplicatesamongthelocalmethodnames,the
interfacenames, andthe fieldnames. Wealsomakesurethatthereisan
initializemethodavailableintheclass.
Foraninterfacedeclaration,weneedonlyprocessthemethodnamesand
theirtypes.
Oncethestaticclassenvironmenthasbeenbuilt, wecancheckeachclass
declaration. This is done by check-class-decl! (figure 9.19). For an
interface, there is nothing to check. For a class declaration, we check each
method, passing along information collected from the static class environ-
ment. Finally,wechecktoseethattheclassactuallyimplementseachofthe
interfacesthatitclaimstoimplement.
To check a method declaration, we first check to see whether its body
matches its declared type. To do this, we build a type environment that
matchestheenvironmentinwhichthebodywillbeevaluated.Wethencheck
toseethattheresulttypeofthebodyisasubtypeofthedeclaredresulttype.
We are not done, however: we have to make sure that if this method is
overriding some method in the superclass, then it has a type that is com-
patible with the superclass method’s type. We have to do this because this
method might be called from a method that knows only about the super-
type. Theonlyexceptiontothisruleisinitialize,whichisonlycalledat
thecurrentclass,andwhichneedstochangeitstypeunderinheritance(see
figure9.12). Todothis,itcallsmaybe-find-method-type,whichreturns
eitherthetypeofthemethodifitisfound,or#fotherwise. Seefigure9.20.
Theprocedurecheck-if-implements?,showninfigure9.21,takestwo
symbols,whichshouldbeaclassnameandaninterfacename. Itfirstchecks
to see that each symbol names what it should name. It then goes through
each method in the interface and checks to see that the class provides a
methodwiththesamenameandacompatibletype.
The static class environment built for the sample program of figure 9.12
isshown infigure9.22. Thestaticclassesareinreverseorder,reflectingthe

| 366 |     |     |     |     |     |     | 9 ObjectsandClasses |
| --- | --- | --- | --- | --- | --- | --- | ------------------- |
→
| add-class-decl-to-static-class-env! |                                     |            |        | : ClassDecl |              | Unspecified |     |
| ----------------------------------- | ----------------------------------- | ---------- | ------ | ----------- | ------------ | ----------- | --- |
| (define                             | add-class-decl-to-static-class-env! |            |        |             |              |             |     |
| (lambda                             | (c-decl)                            |            |        |             |              |             |     |
|                                     | (cases                              | class-decl | c-decl |             |              |             |     |
|                                     | (an-interface-decl                  |            |        | (i-name     | abs-m-decls) |             |     |
(let ((m-tenv
|     |                 | (abs-method-decls->method-tenv |     |      |             |     | abs-m-decls))) |
| --- | --------------- | ------------------------------ | --- | ---- | ----------- | --- | -------------- |
|     | (check-no-dups! |                                |     | (map | car m-tenv) |     | i-name)        |
(add-static-class-binding!
|     |               | i-name | (an-interface |        | m-tenv)))) |          |     |
| --- | ------------- | ------ | ------------- | ------ | ---------- | -------- | --- |
|     | (a-class-decl |        | (c-name       | s-name | i-names    |          |     |
|     |               |        | f-types       |        | f-names    | m-decls) |     |
(let ((i-names
(append
(static-class->interface-names
|     |     |     | (lookup-static-class |     |     | s-name)) |     |
| --- | --- | --- | -------------------- | --- | --- | -------- | --- |
i-names))
(f-names
(append-field-names
(static-class->field-names
|     |     |     | (lookup-static-class |     |     | s-name)) |     |
| --- | --- | --- | -------------------- | --- | --- | -------- | --- |
f-names))
(f-types
(append
(static-class->field-types
|     |     |     | (lookup-static-class |     |     | s-name)) |     |
| --- | --- | --- | -------------------- | --- | --- | -------- | --- |
f-types))
(method-tenv
(let ((local-method-tenv
|     |     |     |     | (method-decls->method-tenv |     |     | m-decls))) |
| --- | --- | --- | --- | -------------------------- | --- | --- | ---------- |
(check-no-dups!
|     |     |     | (map | car local-method-tenv) |     |     | c-name) |
| --- | --- | --- | ---- | ---------------------- | --- | --- | ------- |
(merge-method-tenvs
(static-class->method-tenv
|     |     |     | (lookup-static-class |     |     |     | s-name)) |
| --- | --- | --- | -------------------- | --- | --- | --- | -------- |
local-method-tenv))))
|     | (check-no-dups!            |     |     | i-names | c-name)     |        |         |
| --- | -------------------------- | --- | --- | ------- | ----------- | ------ | ------- |
|     | (check-no-dups!            |     |     | f-names | c-name)     |        |         |
|     | (check-for-initialize!     |     |     |         | method-tenv |        | c-name) |
|     | (add-static-class-binding! |     |     |         |             | c-name |         |
(a-static-class
|     |     | s-name     | i-names                             | f-names |     | f-types | method-tenv))))))) |
| --- | --- | ---------- | ----------------------------------- | ------- | --- | ------- | ------------------ |
|     |     | Figure9.18 | add-class-decl-to-static-class-env! |         |     |         |                    |

9.6 TheTypeChecker 367
→
| check-class-decl! |                    | :          | ClassDecl | Unspecified |                   |     |     |
| ----------------- | ------------------ | ---------- | --------- | ----------- | ----------------- | --- | --- |
| (define           | check-class-decl!  |            |           |             |                   |     |     |
| (lambda           |                    | (c-decl)   |           |             |                   |     |     |
|                   | (cases             | class-decl | c-decl    |             |                   |     |     |
|                   | (an-interface-decl |            |           | (i-name     | abs-method-decls) |     |     |
#t)
|     | (a-class-decl |      | (class-name          |     | super-name  |               | i-names       |
| --- | ------------- | ---- | -------------------- | --- | ----------- | ------------- | ------------- |
|     |               |      | field-types          |     | field-names |               | method-decls) |
|     | (let          | ((sc | (lookup-static-class |     |             | class-name))) |               |
(for-each
|     |     | (lambda             | (method-decl)              |            |             |     |       |
| --- | --- | ------------------- | -------------------------- | ---------- | ----------- | --- | ----- |
|     |     | (check-method-decl! |                            |            | method-decl |     |       |
|     |     |                     | class-name                 | super-name |             |     |       |
|     |     |                     | (static-class->field-names |            |             |     | sc)   |
|     |     |                     | (static-class->field-types |            |             |     | sc))) |
method-decls))
(for-each
|     |     | (lambda               | (i-name) |     |            |     |          |
| --- | --- | --------------------- | -------- | --- | ---------- | --- | -------- |
|     |     | (check-if-implements! |          |     | class-name |     | i-name)) |
i-names)))))
|     |     |     | Figure9.19 | check-class-decl! |     |     |     |
| --- | --- | --- | ---------- | ----------------- | --- | --- | --- |
order in which the class environment is built. Eachof the three classes has
itsmethodsinthesameorder,withthesametype,asdesired.
Thiscompletesthepresentationofthechecker.
(cid:3)
Exercise9.33 [ ] Extend the type checker to enforce the safety property that no
instanceoforcastexpressioniseverperformedonavaluethatisnotanobject,
oronatypethatisnotaclass.
(cid:3)
Exercise9.34 [ ] The expression cast e c cannot succeed unless the type of e is
eitheradescendantoranancestorofc. (Why?) Extendthetypecheckertoguaran-
teethattheprogramneverevaluatesacastexpressionunlessthispropertyholds.
Extendthecheckerforinstanceoftomatch.
(cid:3)
Exercise9.35 [ ] Extend the type checker to enforce the safety property that an
initializemethodiscalledonlyfromwithinanew-object-exp.
Exercise9.36 [ (cid:3) ]Extendthelanguagetoallowinterfacestoinheritfromotherinter-
faces.Aninterfaceshouldrequireallthemethodsrequiredbyallofitsparents.

368 9 ObjectsandClasses
| check-method-decl! |     |           | :   |            |                   |     |              |
| ------------------ | --- | --------- | --- | ---------- | ----------------- | --- | ------------ |
|                    |     | ×         | ×   | ClassName× |                   |     | ×            |
| MethodDecl         |     | ClassName |     |            | Listof(FieldName) |     | Listof(Type) |
→
Unspecified
(define check-method-decl!
| (lambda |                | (m-decl     | self-name | s-name | f-names | f-types)       |       |
| ------- | -------------- | ----------- | --------- | ------ | ------- | -------------- | ----- |
|         | (cases         | method-decl | m-decl    |        |         |                |       |
|         | (a-method-decl |             | (res-type | m-name |         | vars var-types | body) |
|         | (let           | ((tenv      |           |        |         |                |       |
(extend-tenv
|     |     |     | vars var-types |     |     |     |     |
| --- | --- | --- | -------------- | --- | --- | --- | --- |
(extend-tenv-with-self-and-super
|     |     |     | (class-type |     | self-name) |     |     |
| --- | --- | --- | ----------- | --- | ---------- | --- | --- |
s-name
|     |     |     | (extend-tenv |     | f-names | f-types |     |
| --- | --- | --- | ------------ | --- | ------- | ------- | --- |
(init-tenv))))))
|     |     | (let               | ((body-type  | (type-of     | body | tenv)))  |         |
| --- | --- | ------------------ | ------------ | ------------ | ---- | -------- | ------- |
|     |     | (check-is-subtype! |              | body-type    |      | res-type | m-decl) |
|     |     | (if                | (eqv? m-name | ’initialize) |      | #t       |         |
(let ((maybe-super-type
(maybe-find-method-type
(static-class->method-tenv
|     |     |     |     | (lookup-static-class |     |     | s-name)) |
| --- | --- | --- | --- | -------------------- | --- | --- | -------- |
m-name)))
|     |     |     | (if maybe-super-type |     |     |     |     |
| --- | --- | --- | -------------------- | --- | --- | --- | --- |
(check-is-subtype!
|     |     |     | (proc-type       | var-types |     | res-type) |     |
| --- | --- | --- | ---------------- | --------- | --- | --------- | --- |
|     |     |     | maybe-super-type |           |     | body)     |     |
#t)))))))))
check-method-decl!
Figure9.20
(cid:3)(cid:3)
Exercise9.37 [ ]OurlanguageTYPED-OOusesdynamicdispatch. Analternative
designisstaticdispatch.Instaticdispatch,thechoiceofmethoddependsonanobject’s
typeratherthanitsclass.Considertheexample
|     | class        | c1 extends | object     |       |     |     |     |
| --- | ------------ | ---------- | ---------- | ----- | --- | --- | --- |
|     | method       | int        | initialize | () 1  |     |     |     |
|     | method       | int        | m1 () 11   |       |     |     |     |
|     | staticmethod |            | int m2     | () 21 |     |     |     |
|     | class        | c2 extends | c1         |       |     |     |     |
|     | method       | void       | m1 () 12   |       |     |     |     |
|     | staticmethod |            | int m2     | () 22 |     |     |     |

9.6 TheTypeChecker 369
|                      |                      |             |                      | ×             |         | →       |
| -------------------- | -------------------- | ----------- | -------------------- | ------------- | ------- | ------- |
| check-if-implements! |                      | : ClassName |                      | InterfaceName |         | Bool    |
| (define              | check-if-implements! |             |                      |               |         |         |
| (lambda              | (c-name              | i-name)     |                      |               |         |         |
| (cases               | static-class         |             | (lookup-static-class |               |         | i-name) |
|                      | (a-static-class      |             | (s-name              | i-names       | f-names | f-types |
m-tenv)
(report-cant-implement-non-interface
|     | c-name        | i-name))      |     |     |     |     |
| --- | ------------- | ------------- | --- | --- | --- | --- |
|     | (an-interface | (method-tenv) |     |     |     |     |
(let ((class-method-tenv
(static-class->method-tenv
|     |     | (lookup-static-class |     |     | c-name)))) |     |
| --- | --- | -------------------- | --- | --- | ---------- | --- |
(for-each
|     | (lambda | (method-binding) |     |                         |     |     |
| --- | ------- | ---------------- | --- | ----------------------- | --- | --- |
|     |         | (let ((m-name    |     | (car method-binding))   |     |     |
|     |         | (m-type          |     | (cadr method-binding))) |     |     |
(let ((c-method-type
(maybe-find-method-type
class-method-tenv
m-name)))
(if c-method-type
(check-is-subtype!
|     |     |     | c-method-type |     | m-type | c-name) |
| --- | --- | --- | ------------- | --- | ------ | ------- |
(report-missing-method
|     |     |     | c-name | i-name | m-name))))) |     |
| --- | --- | --- | ------ | ------ | ----------- | --- |
method-tenv))))))
|     |          | Figure9.21 | check-if-implements |        |     |     |
| --- | -------- | ---------- | ------------------- | ------ | --- | --- |
| let | f = proc | (x :       | c1) send            | x m1() |     |     |
|     | g = proc | (x :       | c1) send            | x m2() |     |     |
|     | o = new  | c2()       |                     |        |     |     |
| in  | list((f  | o), (g     | o))                 |        |     |     |
Whenfandgarecalled,xwillhavetypec1,butitisboundtoanobjectofclassc2.
Themethodm1usesdynamicdispatch,soc2’smethodform1isinvoked,returning
Themethodm2usesstaticdispatch,sosendinganm2messagetoxinvokesthe
12.
methodassociatedwiththetypeofx,inthiscasec1,so21isreturned.
Modifytheinterpreterofsection9.5tohandlestaticmethods. Asahint,thinkabout
keepingtypeinformationintheenvironmentsothattheinterpretercanfigureoutthe
typeofthetargetexpressioninasend.

| 370 |     |     | 9 ObjectsandClasses |     |
| --- | --- | --- | ------------------- | --- |
((leaf-node
#(struct:a-static-class
object
(tree)
(value)
(#(struct:int-type))
| ((initialize | #(struct:proc-type |     |     |     |
| ------------ | ------------------ | --- | --- | --- |
(#(struct:int-type))
#(struct:void-type)))
| (sum #(struct:proc-type |                      | () #(struct:int-type))) |                         |     |
| ----------------------- | -------------------- | ----------------------- | ----------------------- | --- |
| (getvalue               | #(struct:proc-type   |                         | () #(struct:int-type))) |     |
| (equal                  | #(struct:proc-type   |                         |                         |     |
|                         | (#(struct:class-type |                         | tree))                  |     |
#(struct:bool-type))))))
(interior-node
#(struct:a-static-class
object
(tree)
| (left right)         |                      |                           |        |        |
| -------------------- | -------------------- | ------------------------- | ------ | ------ |
| (#(struct:class-type |                      | tree) #(struct:class-type |        | tree)) |
| ((initialize         | #(struct:proc-type   |                           |        |        |
|                      | (#(struct:class-type |                           | tree)  |        |
|                      | #(struct:class-type  |                           | tree)) |        |
#(struct:void-type)))
| (getleft                | #(struct:proc-type   | ()                      |         |     |
| ----------------------- | -------------------- | ----------------------- | ------- | --- |
|                         | #(struct:class-type  |                         | tree))) |     |
| (getright               | #(struct:proc-type   |                         | ()      |     |
|                         | #(struct:class-type  |                         | tree))) |     |
| (sum #(struct:proc-type |                      | () #(struct:int-type))) |         |     |
| (equal                  | #(struct:proc-type   |                         |         |     |
|                         | (#(struct:class-type |                         | tree))  |     |
#(struct:bool-type))))))
(tree
#(struct:an-interface
| ((sum #(struct:proc-type |                      | () #(struct:int-type))) |        |     |
| ------------------------ | -------------------- | ----------------------- | ------ | --- |
| (equal                   | #(struct:proc-type   |                         |        |     |
|                          | (#(struct:class-type |                         | tree)) |     |
#(struct:bool-type))))))
(object
| #(struct:a-static-class |                                                | #f () () | () ()))) |     |
| ----------------------- | ---------------------------------------------- | -------- | -------- | --- |
| Figure9.22              | Staticclassenvironmentbuiltforthesampleprogram |          |          |     |

9.6 TheTypeChecker 371
(cid:3)(cid:3)
Exercise9.38 [ ] Whymusttheclassinformationbeaddedtothestaticclassenvi-
ronment before the methods are checked? As a hint, consider what happens if a
methodbodyinvokesamethodonself?)
(cid:3)(cid:3)
Exercise9.39 [ ] Make the typechecker prevent calls to initialize other than
theimplicitcallinsidenew.
(cid:3)
Exercise9.40 [ ] Modifythe designof the language so that every field declaration
containsanexpressionthatisusedtoinitializethefield.Suchadesignhastheadvan-
tagethatacheckedprogramwillneverrefertoanuninitializedvalue.
(cid:3)(cid:3)
Exercise9.41 [ ] Extendthetypecheckertohandlefieldrefandfieldset, as
inexercise9.8.
(cid:3)(cid:3)
Exercise9.42 [ ] Inthetypechecker,staticmethodsaretreatedinthesamewayas
ordinarymethods,exceptthatastaticmethodmaynotbeoverriddenbyadynamic
one,orviceversa.Extendthecheckertohandlestaticmethods.

A
For Further Reading
Herearesome of the readingsthattaught, influenced, or inspired usin the
creationofthisbook. Wehopeyouwillenjoyatleastsomeofthemasmuch
aswedid.
Those new to recursive programming and symbolic computation might
look at The Little Schemer(Friedman&Felleisen, 1996), or The Little MLer
(Felleisen&Friedman, 1996), or for the more historically minded, The Lit-
tle LISPer (Friedman, 1974). How to Design Programs (Felleisenetal., 2001)
providesanin-depthtreatmentofhowtoprogramrecursively,intendedasa
firstcourseincomputing.
Usinginductiontodefinesetsandrelationsisalong-standingtechniquein
mathematicallogic. Ourbottom-upandrules-of-inferencestylesarelargely
modeled after the work of Plotkin (1975, 1981). Our “top-down” style is
patternedafteranalternativetechniquecalledcoinduction(seeGordon,1995;
Jacobs&Rutten,1997),usedalsobyFelleisenetal.(2001).
Context-freegrammarsareastandardtoolinbothlinguisticsandcomput-
erscience. Mostcompilerbooks,suchasAhoetal.(2006),haveanextensive
discussionofgrammarsandparsingalgorithms. Theideaofseparatingcon-
creteandabstractsyntaxisusuallycreditedtoMcCarthy(1962),whoempha-
sizedtheuseofaninterfacetomaketheparsetreeabstract.
OurFollowtheGrammarsloganisbasedonstructuralinduction,whichwas
introduced by Burstall (1969). Subgoal induction (Morris&Wegbreit, 1977)
is a useful way of proving the correctness of recursive procedures even if
they do not Follow the Grammar. Subgoal induction also works when an
invariantconstrainsthepossibleinputstotheprocedures.
Generalizationisastandardtechniquefrommathematics,whereoneoften
provesaspecificstatementasaspecialcaseofamoregeneralone. Ourchar-
acterizationofextraargumentsasabstractionsofthecontextismotivatedby
theuseofinheritedattributesinattributegrammars(Knuth,1968).

374 A ForFurtherReading
Our define-datatype and cases constructs were inspired by ML’s
datatype and pattern-matching facilities described in Milneretal. (1989)
anditsrevisionMilneretal.(1997).
The lambda calculus was introduced by Church (1941) to study mathe-
matical logic, but it has become the inspiration for much of the modern
theory of programming languages. Introductory treatments of the lambda
calculusmaybefoundinHankin(1994),PeytonJones(1987),orStoy(1977).
Barendregt(1981,1991)providesanencyclopedicreference.
Contourdiagrams,asinfigure3.13,havebeenusedforexplaininglexical
scopeandwerefirstpresentedbyJohnston(1971). Thenamelessinterpreter
andtranslatorarebasedondeBruijnindices(deBruijn,1972).
Scheme was introduced by Sussman&Steele (1975). Its development is
recordedinSteele&Sussman(1978);Clingeretal.(1985a);Reesetal.(1986);
Clingeretal.(1991);Kelseyetal.(1998). Thestandarddefinitions ofScheme
areprovidedbytheIEEEstandard(IEEE,1991)andtheRevised6Reportonthe
AlgorithmicLanguageScheme(Sperberetal.,2007).
Dybvig(2003)providesashortintroductiontoSchemethatincludesmany
insightfulexamples.
TheideaofaninterpretergoesatleastasfarbackasTuring,whodefined
a“universal”machinethatcouldsimulateanyTuringmachine. Thisuniver-
salmachinewasessentiallyaninterpreterthattookacodeddescriptionofa
Turingmachineandsimulatedtheencodedmachine(Turing,1936). Aclas-
sicalvonNeumannmachine(vonNeumann,1945)islikewiseaninterpreter,
implementedinhardware,thatinterpretsmachinelanguageprograms.
The modern use of interpretersdates backto McCarthy(1960), who pre-
sented a metacircular interpreter (an interpreter written in the defined lan-
guageitself)asanillustrationofthepowerofLisp. Ofcourse,suchaninter-
preterbringswithitanimportantdifficulty:ifalanguageisbeingdefinedin
termsof itself, we need tounderstand the language in orderto understand
the language definition. Indeed, the same problem arises even if the inter-
preterisnotmetacircular. Thereaderstillneedstounderstandthelanguage
inwhichthedefinitioniswrittenbeforeheorshecanunderstandthething
beingdefined.
Overtheyears,avarietyoftechniqueshavebeenusedtoresolvethisdiffi-
culty. Wetreatourinterpretersastranscriptionsofequationalspecifications
(Goguenetal.,1977)orbig-stepoperationalsemanticsinthestyleofPlotkin
(1975,1981). Thisreliesonlyonfairlystraightforwardmathematics.

375
Denotational semantics is another technique that defines a language in
termsofmathematics. Inthisapproach,theinterpreterisreplacedbyafunc-
tionthattranslateseachprograminthedefinedlanguageintoamathemati-
calobjectthatdefinesitsbehavior. Plotkin(1977)providesanindispensable
introduction to this technique, and Winskel (1993) gives a more leisurely
exploration. Milne&Strachey (1976) is an encyclopedic study of how this
techniquecanbeusedtomodelawidevarietyoflanguagefeatures.
Another approach is to write the interpreter in a subset of the language
being defined. For example, our interpretersin chapter 4 relyon Scheme’s
storetoexplaintheconceptofastore,buttheyuseonlyasingleglobalmuta-
bleobject,ratherthanthefullpowerofScheme’smutablevariables.
Theideaofcomputingasmanipulatingastoregoesbacktothebeginning
of modern computing (see vonNeumann, 1945). The design of EXPLICIT-
REFSisbasedonthestoremodelofML(Milneretal.,1989),whichissimilar
to that of Bliss (Wulf, 1971). The design of IMPLICIT-REFS is close to that
ofmost standardprogramminglanguages, suchasPascal, Scheme,or Java,
thathavemutablelocalvariables.
The terms“L-value”and “R-value,”andthe environment-store modelof
memory,areduetoStrachey(1967).
Fortran(Backusetal.,1957)wasthefirstlanguagetousecall-by-reference,
and Algol 60 (Nauretal., 1963) was the first language to use call-by-name.
Friedman&Wise(1976)gaveanearlydemonstrationofthepowerofperva-
sivelazyevaluation. Haskell(Hudaketal.,1990)wasthe firstpracticallan-
guagetousecall-by-need.Plotkin(1975)showedhowtomodelcall-by-value
andcall-by-nameinthelambdacalculus. Tomodelcall-by-name,Ingerman
(1961)inventedthunks. Weusedthemwithaneffecttomodelcall-by-need.
Thisissimilartomemoization(Michie,1968).
Monads, introduced by Moggi (1991) and popularized by Wadler (1992),
provide a systematic model of effectsin programming languages. Monads
provideanorganizingprinciplefornonfunctionalbehaviorinthefunctional
languageHaskell(PeytonJones,2001).
Reynolds(1993)givesafascinatinghistoryoftheseveralindependentdis-
coveriesofcontinuations. Strachey&Wadsworth(1974)isprobablythemost
influential of these. Reynolds (1972) transforms a metacircular interpreter
intoCPSandshowshowdoingthisavoidssomeoftheproblemsofmetacir-
cularity. The translation of programs in tail form to imperative form dates
back to McCarthy (1962) and its importance as a programming technique
wasemphasizedinAbelson&Sussman(1985,1996).

376 A ForFurtherReading
Plotkin (1975) gave a very clean version of the CPS transformation and
workedoutitstheoreticalproperties. Fischer(1972)presentedaverysimilar
version of the transformation. The connection between continuations and
accumulators, as in the fact example at the end of section 6.1, was first
exploredbyWand(1980b).
The idea of making the continuation available to the program goes back
to the J-operator of Landin (1965a) (see also Landin 1965b), and was used
extensively in Lisp and early versions of Scheme (Steele&Sussman, 1978).
Our letcc is based on Scheme’s call-with-current-continuation,
whichfirstappearedinClingeretal.(1985b).
Wand (1980a) showed how continuations could be used as a model for
lightweight processes or threads. Continuations may also be used for a
variety of purposes beyond those discussed in the text, such as coroutines
(Haynesetal.,1986).
Our treatmentof threads approximatesPOSIX threads(see, for example,
Lewis&Berg, 1998). Exercise 5.56 is based on the Erlang message-passing
concurrencymodel(Armstrong,2007).
Steele’sRABBITcompiler(Steele, 1978)usedCPSconversion asthe basis
foracompiler. Inthiscompiler,thesourceprogramwasconvertedintoCPS
andthentransformedtousedata-structurerepresentationsofthecontinua-
tions. Theresultingprogram,likeourregisterizedprograms,couldbecom-
piledeasily.ThislineofdevelopmentledtotheORBITcompiler(Kranzetal.,
1986)andtotheStandardMLofNewJerseycompiler(Appel&Jim,1989).
The CPS algorithm in chapter 6 is based on the first-order composi-
tional algorithm of Danvy&Nielsen (2003). There is a long history of
CPS translations, including Sabry&Wadler (1997), which improved on
Sabry&Felleisen (1993), which in turn was motivated by the CPS algo-
rithmof chapter8ofthe firsteditionof thisbook. Exercise6.30isbasedon
the higher-order compositional CPS algorithm of Danvy&Filinski (1992).
A-normal form (Exercise 6.34) as an alternative to CPS was introduced by
Sabry&Felleisen(1992);Flanaganetal.(1993).
Most current work in typed programming languages can be traced back
toMilner(1978),whointroducedtypesinMLasawayofguaranteeingthe
reliabilityof computer-generatedproofs. Ullman (1998) gives a good short
introduction. A complementary treatment is Felleisen&Friedman (1996);
seealsoPaulson(1996);Smith(2006).
Type inference has been discovered several times. The standard refer-
enceisHindley(1969),thoughHindleyremarksthattheresultswereknown
to Curry in the 1950s. Morris (1968) also proposed type inference, but the
widespreaduseoftypeinferencedidnothappenuntilMilner’s1978paper.

377
Theseparationoftypeinferenceintoequationgenerationandsolvingwas
first articulated by Wand (1987). The system in Milner (1978), known as
Hindley-Milnerpolymorphism,isessentiallythesameasthesystemofexer-
cise7.28. ThetwovolumesofPierce(2002,2004)giveanencyclopedictreat-
mentoftypes.
Theideaofdataabstractionwasaprimeinnovationofthe1970sandhas
alargeliterature,fromwhichwemention onlyParnas(1972)onthe impor-
tanceofinterfacesasboundariesforinformation-hiding. Animplementation
of a data type was any set of values and operations that satisfied the spec-
ification of that data type. Goguenetal. (1977) showed that any data type
could be implemented asa set of treesthatrecordedhow a value was con-
structed, and that there was a unique mapping from such a set of trees to
any other implementation of the data type. Conversely, any data type can
beimplementedusingaproceduralrepresentation,inwhichthedataisrep-
resented by its action under the observers, and in which there is a unique
mappingfromanyotherimplementationofthedatatypetotheprocedural
representation(Giarratanaetal.,1976;Wand,1979;Kamin,1980).
The use of types to enforcedataabstraction appearedin Reynolds (1975)
andtypeswereusedinCLU(Liskovetal.,1977). Thisgrewintothemodule
systemofStandardML(Milneretal.,1989)(seealsoPaulson,1996;Ullman,
1998). OurmodulesystemisbasedonthatofLeroy(1994),whichisusedin
CAML(seeSmith,2006),anothervariationofML.
Simula 67 (Birtwistleetal., 1973) is generally regardedas the first object-
orientedlanguage.Theobject-orientedmetaphorwasextendedbySmalltalk
(Goldberg&Robson, 1983) and by Actors (Hewitt, 1977). Both use human
interactionandsendingandreceivingmessagesasthemetaphorforexplain-
ing their ideas. Scheme grew out of Sussman and Steele’s attempts to
understand Hewitt’s actor model. Abelson&Sussman (1985, 1996) and
Springer&Friedman(1989)providefurtherexamplesofobject-orientedpro-
gramminginSchemeanddiscusswhenfunctionalandimperativeprogram-
ming styles are most appropriate. Steele (1990) and Kiczalesetal. (1991)
describeCLOS,the powerfulobject-orientedprogramming facilityof Com-
monLisp.
Thelanguageinchapter9isbasedontheobjectmodelofJava. Thestan-
dard reference is Arnold&Gosling (1998), but Goslingetal. (1996) is the
specificationfortheseriousreader.

378 A ForFurtherReading
Ruby (see Thomasetal., 2005) Python (vanRossum&Drake, 2006), and
Perl(Walletal.,2000;Dominus,2005),andareuntypedlanguageswithboth
objectsandprocedures,roughlycomparabletoourCLASSES.C#isatyped
languagethataddsmanyfeaturestoJava,mostnotablydelegates,whichare
similartoprocedures,andtheabilityforaprogrammertospecifythatcertain
callsshouldbetailcalls.
Abadi&Cardelli(1996)defineaverysimpleobjectcalculusthatisause-
ful foundation for the study of types in object-oriented systems. Flattetal.
(1998)formalizeasubsetofJava. AnotherusefulsubsetisFeatherweightJava
(Igarashietal.,1999).
Gammaetal. (1995) give a fascinating handbook of useful organizational
principlesforwritingobject-orientedprograms.
The ACM has run three conferences on the history of programming lan-
guages, in 1978 (Wexelblatt, 1978), 1996 (Bergin&Gibson, 1996), and 2007
(Hailpern, 2007). These conferences contain papers describing the history
of a wide varietyof programminglanguages. The IEEEAnnals oftheHisto-
ry of Computing contains scholarly articleson various aspects of computing
history,includingprogramminglanguages.Knuth&Pardo(1977)giveafas-
cinatinghistoryofveryearlyprogramminglanguages.
Therearenumerousconferencesinwhichnewdevelopmentsinprogram-
minglanguagesarereported. Thethreeleadingconferences,atleastforthe
topics discussed in this book, are the ACM Symposium on Principles of Pro-
gramming Languages (POPL), the ACM SIGPLAN International Conference on
Functional Programming (ICFP), and the ACM SIGPLAN Conference on Pro-
gramming LanguageDesign and Implementation(PLDI). Major academic jour-
nalsfor programming languages include ACM Transactions onProgramming
Languages and Systems, the Journal of Functional Programming, and Higher-
Order and Symbolic Computation. In addition to these, there are web sites
devotedtoalmosteveryaspectofprogramminglanguages.

B
The SLLGEN Parsing System
Programs are just strings of characters. In order to process a program, we
needtogroupthesecharactersintomeaningfulunits. Thisgroupingisusu-
allydividedintotwostages: scanningandparsing.
Scanningistheprocessofdividingthesequenceofcharactersintowords,
punctuation, etc. These units are called lexical items, lexemes, or most often
tokens. Parsingistheprocessoforganizingthesequenceoftokensintohier-
archicalsyntacticstructuressuchasexpressions,statements,andblocks. This
ismuchlikeorganizingasentenceintoclauses.
SLLGENis a package for generating scanners and parsersin Scheme. In
thisappendix, we first reviewthe basicsof scanning andparsing, and then
considerhowthesecapabilitiesareexpressedinSLLGEN.
B.1 Scanning
TheproblemofscanningisillustratedinfigureB.1. Thereweshowasmall
portionofaprogram,andthewayinwhichitisintendedtobedividedinto
atomicunits.
Thewayinwhichagivenstreamofcharactersistobeseparatedintolex-
ical items is part of the language specification. This part of the language
specificationissometimescalledthelexicalspecification. Typicalpiecesoflex-
icalspecificationmightbe:
• Anysequenceofspacesandnewlinesisequivalenttoasinglespace.
• Acommentbeginswith%andcontinuesuntiltheendoftheline.
• Anidentifierisasequenceoflettersanddigitsstartingwithaletter.

380 B TheSLLGENParsingSystem
space ignored
comment ignored
ident ident
foo bar %here is a comment
")" "begin" ident
) begin baz
distinguish punctuation, keywords from identifiers
FigureB.1 Thetaskofthescanner
Thejobofthescanneristogothroughtheinputandanalyzeittoproduce
data structures with these items. In a conventional language, the scanner
might be a procedure that, when called, produces the “next” token of the
input.
One could write a scanner from scratch, but that would be tedious and
error-prone. A better approachis to write down the lexical specification in
aspecializedlanguage. Themostcommon languageforthistaskisthelan-
guageofregularexpressions. Wedefinethelanguageofregularexpressionsas
follows:
R::=Character | RR | R ∪ R | R∗ | ¬Character
Each regular expression matches some strings. We can use induction to
definethesetofstringsmatchedbyeachregularexpression:
• Acharactercmatchesthestringconsistingofthecharacterc.
¬
• cmatchesany1-characterstringotherthanc.
• RSmatchesanystringthatconsistsofastringmatching Rfollowedbya
stringmatching S. Thisiscalledconcatenation.
∪
• R SmatchesanystringthateithermatchesRormatchesS.Thisissome-
|
timeswrittenR S,andissometimescalledalternation.

| B.1 Scanning |     |     |     | 381 |
| ------------ | --- | --- | --- | --- |
∗
• R matches any string that is formed by concatenating some number n
| (n ≥ | 0)ofstringsthatmatchR. |     | ThisiscalledtheKleeneclosureof | R.  |
| ---- | ---------------------- | --- | ------------------------------ | --- |
Someexamplesmaybehelpful:
• abmatchesonlythestringab.
∪
• ab cdmatchesthestringsabandcd.
| ∪   | ∪ ∪ |     |     |     |
| --- | --- | --- | --- | --- |
• (ab cd)(ab cd ef)matchesthestringsabab,abcd,abef,cdab,cdcd,
andcdef.
• (ab) ∗ matchestheemptystring,ab,abab,ababab,abababab,....
• (ab ∪ cd) ∗ matches the empty string, ab, cd, abab, abcd, cdab, cdcd,
ababab,...cdcdcd,....
The examplesabove illustrate the precedenceof the differentoperations.
|         | ∗∪          | ∗          |     |     |
| ------- | ----------- | ---------- | --- | --- |
| Thus,ab | cdmeans(a(b | )) ∪ (cd). |     |     |
The specifications for our example may be written using regular expres-
sionsas
|            | = ∪           |                | ∪ ∗      |     |
| ---------- | ------------- | -------------- | -------- | --- |
| whitespace | (space        | newline)(space | newline) |     |
|            | =%( ¬         | ∗              |          |     |
| comment    | newline)      |                |          |     |
|            | =             | ∪ ∗            |          |     |
| identifier | letter(letter | digit)         |          |     |
When scanners use regular expressions to specify a token, the rule is
alwaystotakethelongestmatch. Thiswayxyzwillbescannedasoneiden-
tifier,notthree.
Whenthescannerfindsatoken,itreturnsadatastructureconsistingofat
leastthefollowingpiecesofdata:
• Aclass,whichdescribeswhatkindoftokenithasfound. Thesetofsuch
classesispartofthelexicalspecification. SLLGENusesSchemesymbols
todistinguishtheseclasses;othersyntacticanalyzersmightuseotherdata
structures.
• Apieceofdatadescribingtheparticulartoken. Thenatureofthisdatais
alsopartofthelexicalspecification. Foroursystem,thedataisasfollows:
for identifiers, the data is a Scheme symbol built from the string in the
token; for a number, the datum is the number described by the number
literal;andforaliteralstring,thedatumisthestring. Stringdataareused
for keywordsand punctuation. In animplementation language that did
nothave symbols, one might use a string(the name of the identifier), or
anentryintoahashtableindexedbyidentifiers(asymboltable)instead.

382 B TheSLLGENParsingSystem
• Some data describing the location of this token in the input. This infor-
mation may be used by the parser to help the programmer identify the
locationofsyntacticerrors.
Ingeneral, the internalstructureof tokensis relevantonly tothe scanner
andtheparser,sowewillnotdescribeitinanyfurtherdetail.
B.2 Parsing
Parsing is the process of organizing the sequence of tokens into hierarchi-
calsyntactic structures such asexpressions, statements, and blocks. This is
likeorganizingordiagrammingasentenceintoclauses. Thesyntacticstruc-
tureofalanguageistypicallyspecifiedusingaBNFdefinition,alsocalleda
context-freegrammar(section1.1.2).
Theparsertakesasinputasequenceoftokens,anditsoutputisanabstract
syntaxtree(section2.5). TheabstractsyntaxtreesproducedbyanSLLGEN
parsercanbedescribedbydefine-datatype. Foragivengrammar,there
willbeonedatatypeforeachnonterminal. Foreachnonterminal,therewill
beone variantforeachproductionthathasthe nonterminalasitsleft-hand
side. Each variant will have one field for each nonterminal, identifier, or
number that appears in its right-hand side. A simple example appears in
section2.5. To seewhat happenswhen thereis morethan one nonterminal
inthegrammar,consideragrammarliketheoneinexercise4.22.
Statement ::
={Statement;Statement}
::
=whileExpressiondoStatement
:: = Identifier:=Expression
=
Expression:: Identifier
::
=(Expression-Expression)
Thetreesproducedbythisgrammarcouldbedescribedbythisdatatype:
(define-datatype statement statement?
(compound-statement
(stmt1 statement?)
(stmt2 statement?))
(while-statement
(test expression?)
(body statement?))
(assign-statement
(lhs symbol?)
(rhs expression?)))

|     | B.3 | ScannersandParsersinSLLGEN |     |            |     |             |     |     | 383 |
| --- | --- | -------------------------- | --- | ---------- | --- | ----------- | --- | --- | --- |
|     |     | (define-datatype           |     | expression |     | expression? |     |     |     |
(var-exp
|     |     | (var | symbol?)) |     |     |     |     |     |     |
| --- | --- | ---- | --------- | --- | --- | --- | --- | --- | --- |
(diff-exp
|     |     | (exp1 | expression?)   |     |     |     |     |     |     |
| --- | --- | ----- | -------------- | --- | --- | --- | --- | --- | --- |
|     |     | (exp2 | expression?))) |     |     |     |     |     |     |
For each nonterminal in a right-hand side, the corresponding tree appears
as a field; for each identifier, the corresponding symbol appears as a field.
Thenamesofthevariantswillbespecifiedinthegrammarwhenitiswritten
in SLLGEN. The names of the fields will be automatically generated; here
wehaveintroducedsomemnemonicnamesforthefields. Forexample,the
input
|     | {x  | := foo; | while | x   | do x | := (x | - bar)} |     |     |
| --- | --- | ------- | ----- | --- | ---- | ----- | ------- | --- | --- |
producestheoutput
#(struct:compound-statement
|     |     | #(struct:assign-statement |     |     |     | x   | #(struct:var-exp |     | foo)) |
| --- | --- | ------------------------- | --- | --- | --- | --- | ---------------- | --- | ----- |
#(struct:while-statement
|     |     | #(struct:var-exp          |     |     | x)  |     |     |     |     |
| --- | --- | ------------------------- | --- | --- | --- | --- | --- | --- | --- |
|     |     | #(struct:assign-statement |     |     |     |     | x   |     |     |
#(struct:diff-exp
|     |             |     | #(struct:var-exp |     |        | x)       |     |     |     |
| --- | ----------- | --- | ---------------- | --- | ------ | -------- | --- | --- | --- |
|     |             |     | #(struct:var-exp |     |        | bar))))) |     |     |     |
| B.3 | Scannersand |     | Parsersin        |     | SLLGEN |          |     |     |     |
SpecifyingScanners
In SLLGEN, scanners are specified by regular expressions. Our example
wouldbewritteninSLLGENasfollows:
|     |     | (define     | scanner-spec-a |              |        |         |              |          |         |
| --- | --- | ----------- | -------------- | ------------ | ------ | ------- | ------------ | -------- | ------- |
|     |     | ’((white-sp |                | (whitespace) |        | skip)   |              |          |         |
|     |     | (comment    |                | ("%" (arbno  |        | (not    | #\newline))) | skip)    |         |
|     |     | (identifier |                | (letter      | (arbno |         | (or letter   | digit))) | symbol) |
|     |     | (number     | (digit         |              | (arbno | digit)) | number)))    |          |         |

384 B TheSLLGENParsingSystem
Ifthescannerisusedwithaparserthathaskeywordsorpunctuation,like
while or =, it is not necessary to put these in the scanner manually; the
parser-generatorwilladdthoseautomatically.
AscannerspecificationinSLLGENisalistthatsatisfiesthisgrammar:
|              | =({                  |     | }∗) |
| ------------ | -------------------- | --- | --- |
| Scanner-spec | :: Regexp-and-action |     |     |
}∗)
| Regexp-and-action:: | =(Name | ({ Regexp | Action) |
| ------------------- | ------ | --------- | ------- |
=
| Name   | :: Symbol   |                              |        |
| ------ | ----------- | ---------------------------- | ------ |
| Regexp | :: = String | |letter|digit|whitespace|any |        |
|        | =(not       | Character)|(or               | { }∗)  |
|        | ::          |                              | Regexp |
|        | =(arbno     | Regexp)|(concat              | { }∗)  |
:: Regexp
| Action | :: =skip|symbol|number|string |     |     |
| ------ | ----------------------------- | --- | --- |
Each item in the list is a specification of a regular expression, consisting
of a name, a sequence of regular expressions, and an action to be taken on
success.ThenameisaSchemesymbolthatwillbecometheclassofthetoken.
The second part of the specification is a sequence of regular expressions,
becausethe top levelof a regexp in a scanner is almost always a concatena-
tion. A regular expression may be a string; one of four predefined testers:
letter (matches any letter), digit (matches any digit), whitespace
(matches any Scheme whitespace character), and any (matches any char-
acter); the negation of a character; or it may be a combination of regular
expressions,usingaScheme-likesyntaxwithorandconcatforunionand
concatenation,andarbnoforKleenestar.
Asthescannerworks,itcollectscharactersintoabuffer.Whenthescanner
determines that it has found the longest possible match of all the regular
expressions in the specification, it executes the action of the corresponding
regularexpression.
Anactioncanbeoneofthefollowing:
• The symbol skip. This means this is the end of a token, but no token
is emitted. The scanner continues working onthe stringto find the next
token. Thisactionisusedforwhitespaceandcomments.
symbol.
• The symbol The characters in the buffer are converted into a
Scheme symbol and a token is emitted, with the class name as its class
andwiththesymbolasitsdatum.
• The symbol number. The characters in the buffer are converted into a
Scheme number, and a token is emitted, with the class name as its class
andwiththenumberasitsdatum.

| B.3 ScannersandParsersinSLLGEN |     |     |     |     | 385 |
| ------------------------------ | --- | --- | --- | --- | --- |
• The symbol string. The characters in the buffer are converted into a
Schemestring,andatokenisemitted,withtheclassnameasitsclassand
withthestringasitsdatum.
Ifthereisatieforlongestmatchbetweentworegularexpressions,string
symbol.
| takesprecedenceover |     |     | This rule | means thatkeywordsthat | would |
| ------------------- | --- | --- | --------- | ---------------------- | ----- |
otherwisebeidentifiersaretreatedaskeywords.
SpecifyingGrammars
SLLGEN also includes a language for specifying grammars. The simple
grammarabovewouldbewritteninSLLGENas
| (define | grammar-a1 |     |     |     |     |
| ------- | ---------- | --- | --- | --- | --- |
’((statement
|     | ("{" statement | ";" | statement | "}") |     |
| --- | -------------- | --- | --------- | ---- | --- |
compound-statement)
(statement
|     | ("while" | expression | "do" statement) |     |     |
| --- | -------- | ---------- | --------------- | --- | --- |
while-statement)
(statement
|     | (identifier | ":=" | expression) |     |     |
| --- | ----------- | ---- | ----------- | --- | --- |
assign-statement)
(expression
(identifier)
var-exp)
(expression
|     | ("(" expression |     | "-" expression | ")") |     |
| --- | --------------- | --- | -------------- | ---- | --- |
diff-exp)))
AgrammarinSLLGENisalistdescribedbythefollowinggrammar:
| Grammar      | :: =({ Production | }∗)      |            |     |     |
| ------------ | ----------------- | -------- | ---------- | --- | --- |
|              | =(Lhs             | ({       | }∗)        |     |     |
| Production:: |                   | Rhs-item | Prod-name) |     |     |
=
| Lhs      | :: Symbol           |          |            |            |     |
| -------- | ------------------- | -------- | ---------- | ---------- | --- |
|          | =                   | |        |            |            |     |
| Rhs-item | :: Symbol           | String   |            |            |     |
|          | =(arbno             | {        | }∗)        |            |     |
|          | ::                  | Rhs-item |            |            |     |
|          | :: =(separated-list |          | { Rhs-item | }∗ String) |     |
=
| Prod-name | :: Symbol |     |     |     |     |
| --------- | --------- | --- | --- | --- | --- |
A grammar is a list of productions. The left-hand side of the first pro-
duction is the start symbol for the grammar. Each production consists of a
left-handside(anonterminalsymbol),aright-handside(alistofrhs-item’s)

386 B TheSLLGENParsingSystem
andaproductionname. Theright-handsideofaproductionisalistofsym-
bols or strings. The symbols arenonterminals; strings are literalstrings. A
right-handsidemayalsoincludearbno’sorseparated-list’s;theseare
discussedbelow. Theproductionnameisasymbol,whichbecomesthename
ofthedefine-datatypevariantcorrespondingtotheproduction.
InSLLGEN,thegrammarmustallowtheparsertodeterminewhichpro-
duction to use knowing only (1) what nonterminal it’s looking for and (2)
the first symbol (token) of the string being parsed. Grammars in this form
arecalledLL(1)grammars;SLLGENstandsforSchemeLL(1)parserGENer-
ator. This is somewhat restrictive in practice, but it is good enough for the
purposes of this book. SLLGEN produces a warning if the input grammar
failstomeetthisrestriction.
SLLGENOperations
SLLGEN includes several procedures for incorporating these scanners and
grammarsintoanexecutableparser. FigureB.2showsasampleofSLLGEN
usedtodefineascannerandparserforalanguage.
The procedure sllgen:make-define-datatypes is responsible for
generating a define-datatype expression for each production of the
grammar,forusebycases.Theproceduresllgen:list-define-data-
types generates the define-data-type expressions again, but returns
themasalistratherthanexecutingthem. Thefieldnamesgeneratedbythese
proceduresareuninformativebecausetheinformationisnotinthegrammar;
togetbetterfieldnames,writeoutthedefine-datatype.
The procedure sllgen:make-string-scannertakes a scanner and a
grammarandgeneratesascanningprocedure.Theresultingproceduremay
beappliedtoastringandproducesalistoftokens. Thegrammarisusedto
addkeywordstotheresultingscanningprocedure. Thisprocedureisuseful
primarilyfordebugging.
The proceduresllgen:make-string-parsergeneratesa parser. The
parser is a procedure that takes a string, scans it according to the scanner,
parsesitaccordingtothe grammar, and returnsanabstractsyntax tree. As
with sllgen:make-string-scanner, the literal strings from the gram-
marareincludedinthescanner.
SLLGENcanalsobeusedtobuildaread-eval-print-loop(section3.1).The
proceduresllgen:make-stream-parserislikethestringversion,except
that its input is a stream of charactersand its output is a stream of tokens.
Theproceduresllgen:make-rep-looptakesastring,aprocedureofone

B.3 ScannersandParsersinSLLGEN 387
| (define scanner-spec-1 ...) |     |     |     |     |
| --------------------------- | --- | --- | --- | --- |
(define grammar-1 ...)
| (sllgen:make-define-datatypes | scanner-spec-1 |     | grammar-1) |     |
| ----------------------------- | -------------- | --- | ---------- | --- |
(define list-the-datatypes
(lambda ()
| (sllgen:list-define-datatypes |     | scanner-spec-1 |     | grammar-1))) |
| ----------------------------- | --- | -------------- | --- | ------------ |
(define just-scan
| (sllgen:make-string-scanner | scanner-spec-1 |     | grammar-1)) |     |
| --------------------------- | -------------- | --- | ----------- | --- |
(define scan&parse
| (sllgen:make-string-parser | scanner-spec-1 |     | grammar-1)) |     |
| -------------------------- | -------------- | --- | ----------- | --- |
(define read-eval-print
| (sllgen:make-rep-loop "--> | "   | value-of--program |     |              |
| -------------------------- | --- | ----------------- | --- | ------------ |
| (sllgen:make-stream-parser |     | scanner-spec-1    |     | grammar-1))) |
FigureB.2 UsingSLLGEN
argument,andastreamparser,andproducesaread-eval-printloopthatpro-
ducesthe string asa prompton the standardoutput, readscharactersfrom
thestandardinput,parsesthem,printstheresultofapplyingtheprocedure
totheresultingabstractsyntaxtree,andrecurs.Forexample:
> (define read-eval-print
| (sllgen:make-rep-loop | "--> | " eval-program |     |     |
| --------------------- | ---- | -------------- | --- | --- |
(sllgen:make-stream-parser
scanner-spec-3-1
grammar-3-1)))
> (read-eval-print)
--> 5
5
--> add1(2)
3
--> +(add1(2),-(6,4))
5
The wayin which control is returned fromthis loop to the Scheme read-
eval-printloopissystem-dependent.

388 B TheSLLGENParsingSystem
arbnoandseparated-listPatternKeywords
AnarbnokeywordisaKleenestarinthegrammar: itmatchesanarbitrary
| numberofrepetitionsofitsentry. |     |              |     |     | Forexample,theproduction |     |     |
| ------------------------------ | --- | ------------ | --- | --- | ------------------------ | --- | --- |
| statement::                    | ={{ | statement;}∗ |     | }   |                          |     |     |
couldbewritteninSLLGENas
| (define | grammar-a2 |     |     |     |     |     |     |
| ------- | ---------- | --- | --- | --- | --- | --- | --- |
’((statement
|     | ("{" | (arbno | statement |     | ";") "}") |     |     |
| --- | ---- | ------ | --------- | --- | --------- | --- | --- |
compound-statement)
...))
This makes a compound statement a sequence of an arbitrary number of
semicolon-terminatedstatements.
This arbno generatesa single field in the abstractsyntax tree. This field
will contain a list of the data for the nonterminal inside the arbno. Our
examplegeneratesthefollowingdatatypes:
| (define-datatype |     |     | statement |     | statement? |     |     |
| ---------------- | --- | --- | --------- | --- | ---------- | --- | --- |
(compound-statement
|     | (compound-statement32 |     |     |     | (list-of | statement?))) |     |
| --- | --------------------- | --- | --- | --- | -------- | ------------- | --- |
...)
Asimpleinteractionlookslike:
| >   | (define                    | scan&parse2 |        |      |                |             |              |
| --- | -------------------------- | ----------- | ------ | ---- | -------------- | ----------- | ------------ |
|     | (sllgen:make-string-parser |             |        |      | scanner-spec-a |             | grammar-a2)) |
| >   | (scan&parse2               |             | "{x := | foo; | y := bar;      | z := uu;}") |              |
(compound-statement
|     | ((assign-statement |     |     | x (var-exp | foo))  |     |     |
| --- | ------------------ | --- | --- | ---------- | ------ | --- | --- |
|     | (assign-statement  |     |     | y (var-exp | bar))  |     |     |
|     | (assign-statement  |     |     | z (var-exp | uu)))) |     |     |
Wecanputasequence ofnonterminals insideanarbno. Inthiscase,we
willgetseveralfieldsinthe node, one for eachnonterminal; eachfield will
| containalistofsyntaxtrees. |               |     |              | Forexample: |          |     |     |
| -------------------------- | ------------- | --- | ------------ | ----------- | -------- | --- | --- |
| (define                    | grammar-a3    |     |              |             |          |     |     |
|                            | ’((expression |     | (identifier) |             | var-exp) |     |     |
(expression
|     | ("let" | (arbno |     | identifier | "=" | expression) | "in" expression) |
| --- | ------ | ------ | --- | ---------- | --- | ----------- | ---------------- |
let-exp)))

B.3 ScannersandParsersinSLLGEN 389
(define scan&parse3
(sllgen:make-string-parser scanner-spec-a grammar-a3))
Thisproducesthedatatype
(define-datatype expression expression?
(var-exp (var-exp4 symbol?))
(let-exp
(let-exp9 (list-of symbol?))
(let-exp7 (list-of expression?))
(let-exp8 expression?)))
Hereisanexampleofthisgrammarinaction:
> (scan&parse3 "let x = y u = v in z")
(let-exp
(x u)
((var-exp y) (var-exp v))
(var-exp z))
Thespecification(arbno identifier "=" expression)generatestwo
lists: alistofidentifiersandalistofexpressions. Thisisconvenientbecause
itwillletourinterpretersgetatthepiecesoftheexpressiondirectly.
Sometimesitishelpfulforthesyntaxofalanguagetouselistswithsepa-
rators,notterminators. Thisiscommonenoughthatitisabuilt-inoperation
inSLLGEN.Wecanwrite
(define grammar-a4
’((statement
("{" (separated-list statement ";") "}")
compound-statement)
...))
Thisproducesthedatatype
(define-datatype statement statement?
(compound-statement
(compound-statement103 (list-of statement?)))
...)
Hereisasampleinteraction:
> (define scan&parse4
(sllgen:make-string-parser scanner-spec-a grammar-a4))
> (scan&parse4 "{}")
(compound-statement ())

390 B TheSLLGENParsingSystem
> (scan&parse4 "{x:= y; u := v ; z := t}")
(compound-statement
((assign-statement x (var-exp y))
(assign-statement u (var-exp v))
(assign-statement z (var-exp t))))
> (scan&parse4 "{x:= y; u := v ; z := t ;}")
Error in parsing: at line 1
Nonterminal <seplist3> can’t begin with string "}"
Inthelastexample,theinputstringhadaterminatingsemicolonthatdidnot
matchthegrammar,soanerrorwasreported.
Aswitharbno,wecanplaceanarbitrarysequenceofnonterminalswith-
in a separated-list keyword. In this case, we will get several fields in
the node, one for each nonterminal; each field will contain a list of syntax
trees. This is exactly the same data as would be generated by arbno; only
theconcretesyntaxdiffers.
Wewilloccasionallyusenestedarbno’sandseparated-list’s. Anon-
terminalinsideanarbnogeneratesalist,soanonterminalinsideanarbno
insideanarbnogeneratesalistoflists.
As an example, consider a compound-statement similar to the one in
grammar-a4,exceptthatwehaveparallelassignments:
(define grammar-a5
’((statement
("{"
(separated-list
(separated-list identifier ",")
":="
(separated-list expression ",")
";")
"}")
compound-statement)
(expression (number) lit-exp)
(expression (identifier) var-exp)))
> (define scan&parse5
(sllgen:make-string-parser scanner-spec-a grammar-a5))
Thisgeneratesthefollowingdatatypeforstatement:
(define-datatype statement statement?
(compound-statement
(compound-statement4 (list-of (list-of symbol?)))
(compound-statement3 (list-of (list-of expression?)))))

B.3 ScannersandParsersinSLLGEN 391
Atypicalinteractionlookslike:
> (scan&parse5 "{x,y := u,v ; z := 4; t1, t2 := 5, 6}")
(compound-statement
((x y) (z) (t1 t2))
(((var-exp u) (var-exp v))
((lit-exp 4))
((lit-exp 5) (lit-exp 6))))
Herethecompound-statementhastwofields:alistoflistsofidentifiers,
and the matching list of lists of expressions. In this example we have used
separated-listinsteadofarbno,butanarbnowouldgeneratethesame
data.
(cid:3)
ExerciseB.1 [ ] Thefollowinggrammarforordinaryarithmeticexpressionsbuilds
intheusualprecedencerulesforarithmeticoperators:
Arith-expr ::=Arith-term{Additive-opArith-term}∗
Arith-term ::=Arith-factor{Multiplicative-opArith-factor}∗
Arith-factor ::=Number
::=(Arith-expr)
Additive-op ::=+|-
Multiplicative-op::=
*
|/
This grammar says that every arithmetic expression is the sum of a non-empty
sequence of terms; every term is the product of a non-empty sequence of factors;
andeveryfactoriseitheraconstantoraparenthesizedexpression.
Write a lexical specification and a grammar in SLLGEN that will scan and parse
stringsaccordingtothisgrammar.Verifythatthisgrammarhandlesprecedencecor-
rectly,sothat,forexample3+2*66-5getsgroupedcorrectly,as3+(2×66)−5.
(cid:3)(cid:3)
ExerciseB.2 [ ] Whycan’tthegrammarabovebewrittenwithseparated-list?
(cid:3)(cid:3)
ExerciseB.3 [ ] Define an interpreter that takes the syntax tree producedby the
parserofexerciseB.1andevaluatesitasanarithmeticexpression. Theparsertakes
care of the usual arithmetic precedenceoperations, but the interpreterwill have to
takecareofassociativity,thatis,makingsurethatoperationsatthesameprecedence
level(e.g.additionsand subtractions) are performedfromleftto right. Since there
arenovariablesintheseexpressions,thisinterpreterneednottakeanenvironment
parameter.
(cid:3)(cid:3)
ExerciseB.4 [ ] Extend the language and interpreterof the precedingexerciseto
includevariables.Thisnewinterpreterwillrequireanenvironmentparameter.
(cid:3)
ExerciseB.5 [ ] Addunaryminustothelanguageandinterpreter,sothatinputslike
3*-2arehandledcorrectly.

Bibliography
Abadi, Martín, &Cardelli,Luca.1996. ATheoryofObjects. Berlin,Heidelberg,and
NewYork:Springer-Verlag.
Abelson,Harold,&Sussman,GeraldJay.1985.TheStructureandInterpretationofCom-
puterPrograms. Cambridge,MA:MITPress.
Abelson,Harold,&Sussman,GeraldJay.1996.StructureandInterpretationofComputer
Programs.Secondedition. Cambridge,MA:McGrawHill.
Aho, AlfredV., Lam, Monica S., Sethi, Ravi, & Ullman, JeffreyD. 2006. Compilers:
Principles,Techniques,andTools.Secondedition.Boston:Addison-WesleyLongman.
Appel,AndrewW. &Jim,Trevor.1989.Continuation-Passing,Closure-PassingStyle.
Pages 293–302 of: Proceedings ACM Symposium on Principles of Programming Lan-
guages.
Arnold,Ken,&Gosling,James.1998.TheJavaProgrammingLanguage.Secondedition.
TheJavaSeries. Reading,MA:Addison-Wesley.
Armstrong,Joe.2007. ProgrammingErlang:SoftwareforaConcurrentWorld. ThePrag-
maticProgrammersPublishers.
Backus,JohnW.,etal.1957. TheFortranAutomaticCodingSystem. Pages188–198
of:WesternJointComputerConference.
Barendregt,HenkP.1981. TheLambdaCalculus:ItsSyntaxandSemantics.Amsterdam:
North-Holland.
Barendregt,HenkP.1991. TheLambdaCalculus.Revisededition.StudiesinLogicand
theFoundationsofMathematics,no.103. Amsterdam:North-Holland.
Bergin, Thomas J., & Gibson, Richard G. (eds.). 1996. History of Programming Lan-
guages. NewYork:Addison-Wesley.
Birtwistle, Graham M., Dahl, Ole-Johan, & Myhrhaug, Bjorn. 1973. Simula Begin.
Philadelphia:Auerbach.

394 Bibliography
Burstall,RodM.1969. ProvingPropertiesofProgramsbyStructuralInduction. Com-
puterJournal,12(1),41–48.
Church, Alonzo. 1941. The Calculi of Lambda Conversion. Princeton, NJ: Princeton
UniversityPress. Reprinted1963byUniversityMicrofilms,AnnArbor,MI.
Clinger, William D., et al. 1985a. The Revised Revised Report on Scheme or The
UncommonLisp. Technical MemoAIM-848. Massachusetts Institute of Technol-
ogy,ArtificialIntelligenceLaboratory.
Clinger, William D., Friedman, Daniel P., & Wand, Mitchell. 1985b. A Scheme for
a Higher-Level Semantic Algebra. Pages 237–250 of: Reynolds, John, & Nivat,
Maurice(eds.),AlgebraicMethodsinSemantics:ProceedingsoftheUS-FrenchSeminar
on the Application of Algebra to Language Definition and Compilation (Fontainebleau,
France,June,1982). Cambridge:CambridgeUniversityPress.
Clinger,WilliamD.,Rees,Jonathan,etal.1991. TheRevised4ReportontheAlgorith-
micLanguageScheme. ACMLispPointers,4(3),1–55.
Danvy,Olivier,&Filinski,Andrzej.1992. RepresentingControl: AStudyoftheCPS
Transformation. MathematicalStructuresinComputerScience,2(4),361–391.
Danvy,Olivier,&Nielsen,LasseR.2003.AFirst-orderOne-passCPSTransformation.
TheoreticalComputerScience,308(1-3),239–257.
deBruijn,N.G.1972. LambdaCalculusNotationwithNamelessDummies: ATool
forAutomaticFormulaManipulation,withApplicationtotheChurch-RosserThe-
orem. IndagationesMathematicae,34,381–392.
Dominus,MarkJason.2005. Higher-OrderPerl: TransformingProgramswithPrograms.
SanFrancisco:MorganKaufmannPublishers.
Dybvig,R.Kent.2003. TheSchemeProgrammingLanguage.Thirdedition. Cambridge,
MA:MITPress.
Felleisen, Matthias, & Friedman, Daniel P. 1996. TheLittleMLer. Cambridge, MA:
MITPress.
Felleisen,Matthias,Findler,RobertBruce,Flatt,Matthew,&Krishnamurthi,Shriram.
2001. HowtoDesignPrograms. Cambridge,MA:MITPress.
Fischer, Michael J. 1972. Lambda-Calculus Schemata. Pages 104–109 of: Proceed-
ingsACMConferenceonProvingAssertionsaboutPrograms. RepublishedinLispand
SymbolicComputation,6(3/4),259–288.
Flanagan, Cormac, Sabry, Amr, Duba, Bruce F., & Felleisen, Matthias. 1993. The
Essence of Compiling with Continuations. Pages 237–247 of: Proceedings ACM
SIGPLAN1993Conf.onProgrammingLanguageDesignandImplementation,PLDI’93,
Albuquerque,NM,USA,23–25June1993,vol.28(6). NewYork:ACMPress.
Flatt, Matthew, Krishnamurthi, Shriram, & Felleisen, Matthias. 1998. Classes and
Mixins.Pages171–183of:ProceedingsACMSymposiumonPrinciplesofProgramming
Languages.

Bibliography 395
Friedman, DanielP. 1974. TheLittleLISPer. Palo Alto, CA:Science ResearchAsso-
ciates.
Friedman,DanielP., &Felleisen,Matthias.1996. TheLittleSchemer.Fourthedition.
Cambridge,MA:MITPress.
Friedman,DanielP.,&Wise,DavidS.1976. ConsshouldnotEvaluateitsArguments.
Pages257–284of:Michaelson,S.,&Milner,R.(eds.),Automata,LanguagesandPro-
gramming. Edinburgh:EdinburghUniversityPress.
Gamma,Erich,Helm,Richard,Johnson,Ralph,&Vlissides,John.1995. DesignPat-
terns:ElementsofReusableObject-OrientedSoftware. Reading,MA:AddisonWesley.
Giarratana,V.,Gimona,F.,&Montanari,U.1976. ObservabilityConceptsinAbstract
DataTypeSpecifications. Pages576–587of: Mazurkiewicz,A.(ed.),Mathematical
FoundationsofComputerScience1976. LectureNotesinComputerScience,vol.45.
Berlin,Heidelberg,NewYork:Springer-Verlag.
Goguen, Joseph A., Thatcher, James W., Wagner, Eric G., & Wright, Jesse B. 1977.
InitialAlgebraSemanticsandContinuousAlgebras. JournaloftheACM,24,68–95.
Goldberg,Adele,&Robson,David.1983.Smalltalk-80:TheLanguageandItsImplemen-
tation. Reading,MA:Addison-Wesley.
Gordon,AndrewD.1995. ATutorialonCo-inductionandFunctionalProgramming.
Pages 78–95 of: Functional Programming, Glasgow 1994. Berlin, Heidelberg, and
NewYork:SpringerWorkshopsinComputing.
Gosling,James,Joy,Bill,&Steele,GuyL.1996. TheJavaLanguageSpecification. The
JavaSeries. Reading,MA:Addison-Wesley.
Hailpern,Brent(ed.).2007. HOPLIII:ProceedingsoftheThirdACMSIGPLANConfer-
enceonHistoryofProgrammingLanguages. NewYork:ACMPress.
Hankin,Chris.1994. LambdaCalculi: AGuideforComputerScientists. GraduateTexts
inComputerScience,vol.3. Oxford:ClarendonPress.
Haynes, Christopher T., Friedman, Daniel P., & Wand, Mitchell. 1986. Obtaining
CoroutineswithContinuations. J.ofComputerLanguages,11(3/4),143–153.
Hewitt,Carl.1977.ViewingControlStructuresasPatternsofPassingMessages.Arti-
ficialIntelligence,8,323–364.
Hindley,Roger.1969. ThePrincipalType-SchemeofanObjectinCombinatoryLogic.
TransactionsoftheAmericanMathematicalSociety,146,29–60.
Hudak,Paul,etal.1990.ReportontheProgrammingLanguageHASKELL.Technical
ReportYALEU/DCS/RR-777.YaleUniversity,CSDept.
IEEE.1991. IEEEStandardfortheSchemeProgrammingLanguage,IEEEStandard1178-
1990. IEEEComputerSociety,NewYork.

396 Bibliography
Igarashi,Atshushi,Pierce,BenjaminC.,&Wadler,Philip.1999. FeatherweightJava:
AMinimalCoreCalculusforJavaandGJ.Pages132–146of:Meissner,Loren(ed.),
Proceedingsofthe1999 ACMSIGPLANConferenceonObject-OrientedProgramming,
Systems,Languages&Applications(OOPSLA‘99).
Ingerman, PeterZ.1961. Thunks, AWayofCompilingProcedureStatements with
SomeCommentsonProcedureDeclarations. CommunicationsoftheACM,4(1),55–
58.
Jacobs, Bart, & Rutten, Jan. 1997. A Tutorial on (Co)Algebras and (Co)Induction.
BulletinoftheEuropeanAssociationforTheoreticalComputerScience,62,222–259.
Johnston,JohnB.1971. TheContourModelofBlockStructuredProcesses. SIGPLAN
Notices,6(2),55–82.
Kamin,Samuel.1980. FinalDataTypeSpecifications:ANewDataTypeSpecification
Method. Pages131–138 of: ProceedingsACMSymposiumonPrinciplesofProgram-
mingLanguages.
Kelsey, Richard, Clinger, William D., & Rees, Jonathan. 1998. Revised5 Report on
theAlgorithmicLanguageScheme. Higher-OrderandSymbolicComputation,11(1),
7–104.
Kiczales,G.,desRivières,J.,&Bobrow,D.G.1991. TheArtoftheMeta-ObjectProtocol.
Cambridge,MA:MITPress.
Knuth,DonaldE.1968. SemanticsofContext-FreeLanguages. MathematicalSystems
Theory,2,127–145. Correction,5:95–96,1971.
Knuth,DonaldE.,&Pardo,L.T.1977. TheEarlyDevelopmentofProgrammingLan-
guages.Pages419–493of:Belzer,J.,Holzman,A.G.,&Kent,D.(eds.),Encyclopedia
ofComputerScienceandTechnology,vol.6. NewYork:MarcelDekker.
Kranz, David A., Kelsey, Richard, Rees, Jonathan A., Hudak, Paul, Philbin, James,
& Adams, Norman I. 1986. Orbit: An Optimizing Compilerfor Scheme. Pages
219–223of:ProceedingsSIGPLAN’86SymposiumonCompilerConstruction.
Landin,PeterJ.1965a. CorrespondencebetweenALGOL60andChurch’sLambda-
notation:PartI. Commun.ACM,8(2),89–101.
Landin, Peter J. 1965b. A Generalization of Jumps and Labels. Technical Report.
UNIVACSystemsProgrammingResearch. ReprintedwithaforewordinHigher-
OrderandSymbolicComputation,11(2):125–143,1998.
Leroy, Xavier. 1994. Manifest Types, Modules, and Separate Compilation. Pages
190–122of:ProceedingsACMSymposiumonPrinciplesofProgrammingLanguages.
Lewis,Bil,&Berg,DanielJ.1998. MultithreadedProgrammingwithPThreads. Engle-
woodCliffs,NJ:Prentice-Hall.
Liskov, Barbara, Snyder, Alan, Atkinson, R., & Schaffert, Craig. 1977. Abstraction
MechanismsinCLU. CommunicationsoftheACM,20,564–576.

Bibliography 397
McCarthy,John.1960. RecursiveFunctionsofSymbolicExpressionsandtheirCom-
putationbyMachine,PartI. CommunicationsoftheACM,3,184–195.
McCarthy,John.1962. TowardsaMathematicalScienceofComputation.Pages21–28
of:Popplewell(ed.),InformationProcessing62. Amsterdam:North-Holland.
Michie,Donald.1968. “Memo”FunctionsandMachineLearning. Nature,218(1–3),
218–219.
Milne, Robert, & Strachey, Christopher. 1976. A Theory of Programming Language
Semantics. London:ChapmanandHall.
Milner, Robin. 1978. A Theoryof Type PolymorphisminProgramming. Journalof
ComputerandSystemsScience,17,348–375.
Milner, Robin, Tofte, Mads, & Harper, Robert. 1989. The Definition of Standard ML.
Cambridge,MA:MITPress.
Milner,Robin,Tofte,Mads,Harper,Robert,&MacQueen,DavidB.1997.TheStandard
MLProgrammingLanguage(Revised). Cambridge,MA:MITPress.
Moggi,Eugenio.1991. NotionsofComputationandMonads.InformationandCompu-
tation,93(1),55–92.
Morris, Jr., James H. 1968. Lambda Calculus Models of Programming Languages.
Ph.D.thesis,MIT,Cambridge,MA.
Morris,Jr.,JamesH., &Wegbreit,Ben.1977. SubgoalInduction. Communicationsof
theACM,20,209–222.
Naur, Peter, et al. 1963. Revised Report on the Algorithmic Language ALGOL 60.
CommunicationsoftheACM,5(1),1–17.
Parnas,DavidL.1972. ATechniqueforModuleSpecificationwithExamples. Com-
municationsoftheACM,15(5),330–336.
Paulson, Laurence C. 1996. ML for the Working Programmer. Second edition. New
York:CambridgeUniversityPress.
PeytonJones,SimonL.1987. TheImplementationofFunctionalProgrammingLanguages.
EnglewoodCliffs,NJ:Prentice-HallInternational.
PeytonJones,SimonL.2001. TacklingtheAwkwardSquad:MonadicInput/Output,
Concurrency, Exceptions, and Foreign-Language Calls in Haskell. In: Hoare,
C.A.R., Broy, Manfred, & Steinbruggen, Ralf (eds.), Engineering Theories of Soft-
wareConstruction,MarktoberdorfSummerSchool.Amsterdam,TheNetherlands:IOS
Press.
Pierce, Benjamin C. 2002. Types and Programming Languges. Cambridge, MA: MIT
Press.
Pierce,BenjaminC.2004. AdvancedTopicsinTypesandProgrammingLanguges. Cam-
bridge,MA:MITPress.

398 Bibliography
Plotkin,GordonD.1975.Call-by-Name,Call-by-Valueandtheλ-Calculus.Theoretical
ComputerScience,1,125–159.
Plotkin,GordonD.1977. LCFConsideredasaProgrammingLanguage. Theoretical
ComputerScience,5,223–255.
Plotkin,GordonD.1981. AStructuralApproachtoOperationalSemantics. Technical
Report FN 19, DAIMI, Department of Computer Science. University of Aarhus,
Aarhus,Denmark.
Pratt,TerrenceW.,&Zelkowitz,MarvinV.2001. ProgrammingLanguages: Designand
Implementation.4thedition. EnglewoodCliffs,NJ:Prentice-Hall.
Rees,JonathanA.,Clinger,WilliamD.,etal.1986.Revised3ReportontheAlgorithmic
LanguageScheme. SIGPLANNotices,21(12),37–79.
Reynolds, John C. 1972. Definitional Interpreters for Higher-Order Programming
Languages. Pages 717–740 of: Proceedings ACM National Conference. Reprinted,
withaforeword,inHigher-OrderandSymbolicComputation11(4)363-397(1998).
Reynolds,JohnC.1975. User-DefinedTypesandProceduralDataStructuresasCom-
plementaryApproachesto DataAbstraction. In: ConferenceonNewDirectionson
AlgorithmicLanguages. IFIPWP2.1,Munich.
Reynolds,JohnC.1993. TheDiscoveriesofContinuations. LispandSymbolicCompu-
tation,6(3/4),233–248.
Sabry,Amr,&Felleisen,Matthias.1992. ReasoningaboutProgramsinContinuation-
PassingStyle. Pages288–298of:Proceedings1992ACMConf.onLispandFunctional
Programming. NewYork:ACMPress.
Sabry,Amr,&Felleisen,Matthias.1993. ReasoningaboutProgramsinContinuation-
PassingStyle. LispandSymbolicComputation,6(3/4),289–360.
Sabry,Amr,&Wadler,Philip.1997. AReflectiononCall-by-Value. ACMTransactions
onProgrammingLanguagesandSystems,19(6),916–941.
Scott,MichaelL.2005. ProgrammingLanguagePragmatics.Secondedition. SanFran-
cisco:MorganKaufmann.
Sebesta, Robert W. 2007. Concepts of Programming Languages. 8th edition. Boston:
Addison-WesleyLongmanPublishingCo.,Inc.
Smith,JoshuaB.2006. PracticalOCaml. Berkeley,CA:Apress.
Sperber, Michael, Dybvig, R. Kent, Flatt, Matthew, & van Straaten, Anton. 2007.
Revised6ReportontheAlgorithmicLanguageScheme. www.r6rs.org.
Springer, George, & Friedman, Daniel P. 1989. Scheme and the Art of Programming.
NewYork:McGraw-Hill.
Steele,GuyL.1978.Rabbit:ACompilerforScheme.ArtificialIntelligenceLaboratory
TechnicalReport474.MassachusettsInstituteofTechnology,Cambridge,MA.

Bibliography 399
Steele, Guy L. 1990. Common Lisp: the Language. Second edition. Burlington, MA:
DigitalPress.
Steele,GuyL.,&Sussman,GeraldJay.1978. TheRevisedReportonSCHEME. Arti-
ficial Intelligence Memo 452. Massachusetts Institute of Technology, Cambridge,
MA.
Stoy,JosephE.1977. DenotationalSemantics: TheScott-StracheyApproachtoProgram-
mingLanguageTheory. Cambridge,MA:MITPress.
Strachey,Christopher.1967. FundamentalConceptsinProgrammingLanguages.Unpub-
lished notes from International Summer School on Programming Languages,
Copenhagen. Reprinted, with aforeword,inHigher-Order andSymbolicComputa-
tion13(1–2)11–49(2000).
Strachey,Christopher,&Wadsworth,ChristopherP.1974. Continuations: AMathe-
maticalSemanticsforHandlingFullJumps.TechnicalMonographPRG-11.Oxford
UniversityComputingLaboratory.Reprinted,withaforeword,inHigher-Orderand
SymbolicComputation13(1–2)135–152(2000).
Sussman, Gerald J., & Steele, Guy L. 1975. SCHEME: An Interpreter for Extend-
ed Lambda Calculus. Artificial Intelligence Memo 349. Massachusetts Institute
ofTechnology,Cambridge,MA. Reprinted,withaforeword,inHigher-Orderand
SymbolicComputation11(4)405-439(1998).
Thomas,Dave,Fowler,Chad,&Hunt,Andy.2005. ProgrammingRuby:ThePragmatic
Programmers’Guide.Secondedition. Raleigh,NC:ThePragmaticBookshelf.
Turing,A.M.1936. OnComputableNumbers,withanApplicationtotheEntschei-
dungsproblem.Proc.LondonMath.Soc.,42(1),230–265.
Ullman, Jeffrey D. 1998. Elements of ML Programming. ML97 edition. Englewood
Cliffs,NJ:Prentice-Hall.
vanRossum,Guido,&Drake,FredL.Jr.2006. ThePythonLanguageReferenceManual
(Version2.5). Bristol,UK:NetworkTheoryLtd.
vonNeumann,John.1945. FirstDraftofaReportontheEDVAC. TechnicalReport.
MooreSchoolofElectricalEngineering,UniversityofPennsylvania.
Wadler,Philip.1992. TheEssenceofFunctional Programming. Pages1–14 of: Pro-
ceedingsACMSymposiumonPrinciplesofProgrammingLanguages.
Wall,Larry,Christiansen,Tom,&Orwant,Jon.2000. ProgrammingPerl.3rdedition.
Cambridge,MA:O’Reilly.
Wand,Mitchell.1979. FinalAlgebraSemanticsandDataTypeExtensions. Journalof
ComputerandSystemsScience,19,27–44.
Wand,Mitchell.1980a.Continuation-BasedMultiprocessing.Pages19–28of:Allen,J.
(ed.),ConferenceRecordofthe1980LISPConference.PaloAlto,CA:TheLispCompa-
ny.RepublishedbyACM.Reprinted,withaforeword,inHigher-OrderandSymbolic
Computation12(3)285–299(1999).

400 Bibliography
Wand,Mitchell.1980b.Continuation-BasedProgramTransformationStrategies.Jour-
naloftheACM,27,164–180.
Wand,Mitchell.1987. ASimpleAlgorithmandProofforTypeInference. Fundamenta
Informaticae,10,115–122.
Wexelblatt,R.L.(ed.).1978.SpecialIssue:HistoryofProgrammingLanguagesConference.
Vol.13. NewYork:ACMPress.
Winskel,Glynn.1993. TheFormalSemanticsof ProgrammingLanguages. Cambridge,
MA:MITPress.
Wulf,William.1971. BLISS:ALanguageforSystemsProgramming. Communications
oftheACM,14(12),780–790.

Index
| Abadi,Martin,378,393       |     |     | Armstrong,Joe,376,393 |     |     |
| -------------------------- | --- | --- | --------------------- | --- | --- |
| Abelson,Harold,375,377,393 |     |     | Arnold,Ken,377,393    |     |     |
Abstract data types (ADTs), 31, 377. Arrays,128–130(ex. 4.29–30),135(ex.
SeealsoRecursivedatatypes
4.36)
Abstraction boundary, 275, 278, 296, Assignment, 103, 122 (ex. 4.21). See
377
alsoMutation
Abstractsyntax,51–53,371
Associationlist(a-list),39(ex.2.5,2.8–
| Abstractsyntaxtree,51–53,57–58,382 |     |     | 10) |     |     |
| ---------------------------------- | --- | --- | --- | --- | --- |
Abstracttype,34,292,296–300,326
Atkinson,R.,396
| Accumulator,203,376        |     |         | Auxiliaryprocedures,22-25 |     |     |
| -------------------------- | --- | ------- | ------------------------- | --- | --- |
| Actionunderapplication,41. |     | Seealso |                           |     |     |
Axiom,3
Proceduralrepresentation
| Activation record, | 155 (ex. 5.15), | 189 |     |     |     |
| ------------------ | --------------- | --- | --- | --- | --- |
Backus,John,375,393
(ex.5.49)
Backus-NaurForm(BNF),6
Actualparameter,75
Barendregt,Henk,374,393
Adams,Norman,396
|     |     |     | begin expression, | 105, 108 | (ex. 4.4), |
| --- | --- | --- | ----------------- | -------- | ---------- |
Aho,Alfred,371,393
|     |     |     | 112 (ex. 4.10), | 153(ex. 5.11), | 231 |
| --- | --- | --- | --------------- | -------------- | --- |
AlgorithmW,274(ex.7.29)
(ex.6.36),334
Aliases,133
| Allocation |     |     | Berg,Daniel,376,396 |     |     |
| ---------- | --- | --- | ------------------- | --- | --- |
Bergin,Thomas,378,393
ofobjects,335,337,339,362
β-reduction,138
instore,104,108–109,113,229–231
| Alternation,380.SeealsoOr    |      |       | Bidirectionalsequences,44(ex.2.18)    |           |         |
| ---------------------------- | ---- | ----- | ------------------------------------- | --------- | ------- |
| Ancestorclass,329            |      |       | Bignumrepresentationofnaturalnum-     |           |         |
| A-normalform(ANF),226        | (ex. | 6.34– | bers,34                               |           |         |
| 35),376                      |      |       | Binarymethodproblem,350(ex.9.25)      |           |         |
| Antecedent,3                 |      |       | Binarysearchtree(Binary-search-tree), |           |         |
| a(n)-type-nameconstructor,48 |      |       | 10,30(ex.1.34)                        |           |         |
| Appel,Andrew,376,393         |      |       | Binarysemaphore,187–189,190           |           |         |
| apply-env,36,38,40           |      |       | Binary tree (Bintree),                | 9, 11–12, | 29 (ex. |
apply-procedures,41
|             |     |     | 1.31–33), 30            | (ex. 1.35), 44-45 | (ex. |
| ----------- | --- | --- | ----------------------- | ----------------- | ---- |
| Argument,75 |     |     | 2.19–20),50(ex.2.24–25) |                   |      |

402 Index
| Binding                             | Casting,356                        |                    |               |
| ----------------------------------- | ---------------------------------- | ------------------ | ------------- |
| inenvironment,36                    | CHECKED,240–243,244,245,246        |                    |               |
| extentof,90,168(ex.5.30)            | Childclass,329                     |                    |               |
| fluid,122(ex.4.21)                  | Christiansen,Tom,399               |                    |               |
| lambda,10,18–19                     | Church,Alonzo,374,394              |                    |               |
| let,65–67,90,118,119                | Classenvironment,336,342–344,346,  |                    |               |
| letrec,82–83,90,119                 | 358–359,362,365–367,370            |                    |               |
| inmodule,278                        | Classes,342–344,346                |                    |               |
| proc,75,90,118,119                  | declarationof,326,334,365,367      |                    |               |
| ofpseudo-variables,336              | host,331,342                       |                    |               |
| oftypevariables,252,260             | parent,329                         |                    |               |
| ofvariables,87–91,103               | subclass,329                       |                    |               |
| Birtwistle,Graham,377,393           | superclass,329,331                 |                    |               |
| Bobrow,Daniel,396                   | Classvariables,348–349(ex.9.15)    |                    |               |
| Body                                | CLASSES,334–346                    |                    |               |
| let,65–67,90,118,119                | ClientofADT,31,32                  |                    |               |
| letrec,82–83,90,119                 | Clinger,                           | William, 374, 376, | 394, 396,     |
| ofmethod,327                        | 398                                |                    |               |
| ofmodule,278,283,319                | Closures,80,                       | 85–87 (ex.         | 3.35–36), 121 |
| ofmoduleprogram,278                 | (ex.4.19)                          |                    |               |
| proc,75,90,118,119                  | Coinduction,371                    |                    |               |
| booltype,237                        | Commandcontinuations,155(ex.5.16)  |                    |               |
| Booleanexpressions(Bool-exp),73(ex. | Compiler,58                        |                    |               |
| 3.14)                               | Concatenation,380                  |                    |               |
| Bottom-updefinition,3,371           | Conclusion,3                       |                    |               |
| Boundvariable,10,75                 | condexpression,73(ex.3.12),101(ex. |                    |               |
| deBruijnindices,91–93,349(ex.9.19–  | 3.38)                              |                    |               |
| 20),374                             | Concretesyntax,51–53,371           |                    |               |
| deBruijn,N.G.,394                   | Concretetypes,34,292,294–295       |                    |               |
| Burstall,Rod,371,394                | Conditionals,                      | 63, 65, 146,       | 221 (ex.      |
| Bytecode,58                         | 6.23),243(ex.7.7)                  |                    |               |
Consequent,3
Call-by-name,137–138,375
Constructors,33,43
| Call-by-need,137–138,375 | Contextargument,23–24 |     |     |
| ------------------------ | --------------------- | --- | --- |
Call-by-reference,130–133,375
Context-freegrammar,10,371,382
| Call-by-value,117,130                 | Context-sensitiveconstraint,10–11,327, |     |     |
| ------------------------------------- | -------------------------------------- | --- | --- |
| Call-by-value-result,135–136(ex.4.37) | 371                                    |     |     |
call-with-current-continuation,
Continuation-passingstyle,141,193
| 178(ex.5.42–44),376   | examplesof,193–200                |     |     |
| --------------------- | --------------------------------- | --- | --- |
| Cardelli,Luca,378,393 | transformationto,200,212–220,222– |     |     |
casesform,46,49,50(ex.2.25),374
224,375,376

| Index                         |     |     |                   |     | 403 |
| ----------------------------- | --- | --- | ----------------- | --- | --- |
| Continuations,141–153,156,375 |     |     | ofvariables,87–91 |     |     |
command continuations, 155 (ex. Deduction,5,70(ex.3.4),72(ex.3.5)
define-datatypeform,46–50,374
5.16)
| datastructurerepresentationof,146, |                |                | Definedlanguage,57,374              |     |     |
| ---------------------------------- | -------------- | -------------- | ----------------------------------- | --- | --- |
| 148,                               | 153 (ex. 5.2), | 163, 164, 194, | Defininglanguage,57,374             |     |     |
| 201(ex.6.4),225(ex.6.31)           |                |                | Defunctionalization,41,155,157–159, |     |     |
procedural representation of, 146, 160 (ex. 5.22), 169(ex. 5.33), 171
| 147,                   | 153 (ex. 5.1), | 178 (ex. 5.41), | (ex.5.34)                 |     |     |
| ---------------------- | -------------- | --------------- | ------------------------- | --- | --- |
| 194,198,201(ex.6.4)    |                |                 | Delegates,378             |     |     |
| Contourdiagrams,89,374 |                |                 | Denotationalsemantics,375 |     |     |
| Contract,1,13          |                |                 | Denotedvalues,61          |     |     |
Contravariant subtyping, 321, 361– Dereferencing,104,109,113,229–231
| 362,363 |     |     | Derivation,syntactic,8 |     |     |
| ------- | --- | --- | ---------------------- | --- | --- |
Controlcontext,139–141,144,162,203 Derivationtree,5,70(ex. 3.4),72(ex.
| Covariantsubtyping,361–362,363 |                |                | 3.5)                            |                  |      |
| ------------------------------ | -------------- | -------------- | ------------------------------- | ---------------- | ---- |
| CPS-IN,203,204                 |                |                | Descendantclass,329             |                  |      |
| CPS-OUT,206,208                |                |                | Differenceexpressions,62–63,149 |                  |      |
| CPSRecipe,200                  |                |                | Diff-trees(Diff-tree),34        |                  |      |
| Criticalregion,187–188         |                |                | Domain-specific                 | languages, x–xi, | xii- |
| Curry,Haskell,376              |                |                | xiii,49–50,53                   |                  |      |
| Currying,                      | 80 (ex. 3.20), | 81 (ex. 3.23), | Dominus,Mark,378,394            |                  |      |
| 301(ex.8.15)                   |                |                | Dotnotation,4                   |                  |      |
Doubledispatch,357(ex.9.32)
| Dahl,Ole-Johan,393 |     |     | do-whilestatement,123(ex.4.24) |     |     |
| ------------------ | --- | --- | ------------------------------ | --- | --- |
Danvy,Olivier,376,394
Drake,Fred,399
| Dataabstraction,31,377 |     |     | Duba,Bruce,394 |     |     |
| ---------------------- | --- | --- | -------------- | --- | --- |
Datastructurerepresentation
Dybvig,R.Kent,374,394,398
| of continuations, | 146, | 148, 153 (ex. | Dynamicassignment,122 |     |     |
| ----------------- | ---- | ------------- | --------------------- | --- | --- |
5.2), 163, 164, 194, 201 (ex. 6.4), Dynamicbinding(dynamicscope),82
225(ex.6.31)
|                  |         |                | (ex. 3.28–29),      | 87 (ex. 3.37), | 168 |
| ---------------- | ------- | -------------- | ------------------- | -------------- | --- |
| of environments, | 37–38,  | 39–40 (ex.     | (ex.5.30)           |                |     |
| 2.5–11)          |         |                | Dynamicdispatch,332 |                |     |
| of procedure     | values, | 79–80, 81 (ex. |                     |                |     |
Dynamicextent,168(ex.5.30)
| 3.26),82(ex. | 3.28),101(ex. | 3.42), | Dynamicpropertiesofprograms,90– |     |     |
| ------------ | ------------- | ------ | ------------------------------- | --- | --- |
225(ex.6.31)
91
ofthreads,189(ex.5.48)
| oftrampolining,160(ex.5.18–20) |     |     | Eagerevaluation,136    |           |      |
| ------------------------------ | --- | --- | ---------------------- | --------- | ---- |
| Declaration                    |     |     | Effects,computational, | 103, 109, | 226– |
| ofclasses,326,334,365,367      |     |     | 232,274(ex.7.30),375   |           |      |
| ofmethod,334,365,368           |     |     | empty-env,36,38,40     |           |      |
of procedures,75–77, 80 (ex. 3.19), Environment ADT (Env), 35–41, 50
| 101(ex.3.43–44),214 |     |     | (ex.2.21) |     |     |
| ------------------- | --- | --- | --------- | --- | --- |

404 Index
| Environments,35,61–62               |                |             | Fibonacci sequence,             | 198–199,  | 226 (ex.  |
| ----------------------------------- | -------------- | ----------- | ------------------------------- | --------- | --------- |
| association-listrepresentationof,39 |                |             | 6.34)                           |           |           |
| (ex.2.5,2.8–10)                     |                |             | Fieldofobject,325,326,340–342   |           |           |
| classenvironment,336,342–344,346,   |                |             | Filinski,Andrzej,376,394        |           |           |
| 358–359,362,365–367,370             |                |             | Findler,Robert,394              |           |           |
| datastructurerepresentationof,37–   |                |             | Fischer,Michael,376,394         |           |           |
| 38,39–40(ex.2.5–11)                 |                |             | Flanagan,Cormac,376,394         |           |           |
| formethodcall,340–342               |                |             | Flatt,Matthew,378,394,398       |           |           |
| methodenvironments,345              |                |             | Fluidbinding,122(ex.4.21)       |           |           |
| nameless,98–99                      |                |             | FollowtheGrammar,22,371         |           |           |
| proceduralrepresentationof,40–41,   |                |             | examplesof,12–21                |           |           |
| 42(ex.2.12–14),85(ex.3.34)          |                |             | Formalparameter,75              |           |           |
| ribcage                             | representation | of, 40, 101 | Fowler,Chad,399                 |           |           |
| (ex.3.41)                           |                |             | Frame,155(ex.5.15),189(ex.5.49) |           |           |
| static,94–96                        |                |             | Freeoccurrenceofvariable,18     |           |           |
| typeenvironment,239                 |                |             | Freeze,136                      |           |           |
| eopl:errorprocedure,15              |                |             | Friedman, Daniel,               | 371, 375, | 376, 377, |
| Equationalspecification,65,374      |                |             | 394,395,398                     |           |           |
Errorhandling,15
Gamma,Erich,378,395
| Exceptionhandling,171–177, |     | 202(ex. |                             |     |     |
| -------------------------- | --- | ------- | --------------------------- | --- | --- |
| 6.8),232                   |     |         | Generalization,22–23,24,371 |     |     |
Giarratana,V.,377,395
Executionforeffect,109
Gibson,Richard,378,393
Expandedtype,303–307
| EXPLICIT-REFS,104–111,229–231,248 |     |     | Gimona,F,395 |     |     |
| --------------------------------- | --- | --- | ------------ | --- | --- |
Goguen,Joseph,374,377,395
(ex.7.10),272(ex.7.26),375
Goldberg,Adele,377,395
Expressedvalues,61,73(ex.3.13)
Gordon,Andew,371,395
Expressions
Gosling,James,377,393,395
LET,62–63,65–67
goto,162
simple,206,226
Grammars,6–11,371,382
tailform,203–207,375
extend-env,36,38,40
Hailpern,Brent,378,395
| extend-env*, | 36, 38, 39–40 | (ex. 2.10– |     |     |     |
| ------------ | ------------- | ---------- | --- | --- | --- |
Hankin,Chris,374,395
11),342
Harper,Robert,397
extend-env-rec,83,85–86(ex.3.35)
Haynes,Christopher,395
| Extentofvariablebinding,90,168(ex. |     |     | Helm,Richard,395 |     |     |
| ---------------------------------- | --- | --- | ---------------- | --- | --- |
5.30)
Hewitt,Carl,377,395
Extractors,44–43
Hindley,Roger,376,395
Hostclass,331,342
| Factorialfunction,34(ex. |     | 2.1),81(ex. |     |     |     |
| ------------------------ | --- | ----------- | --- | --- | --- |
Hudak,Paul,375,395,396
| 3.23), | 87 (ex. 3.37), | 139–140, 153 |     |     |     |
| ------ | -------------- | ------------ | --- | --- | --- |
Hunt,Andy,399
| (ex. | 5.13–14), 162, | 168 (ex. 5.29), |     |     |     |
| ---- | -------------- | --------------- | --- | --- | --- |
Hypothesis,3
193–197,202–203,204–205
| Felleisen,Matthias,371,376,394,395, |     |     | Icecreamsundaes,322       |     |     |
| ----------------------------------- | --- | --- | ------------------------- | --- | --- |
| 398                                 |     |     | Igarashi,Atshushi,378,396 |     |     |

| Index                     |     |     |                       |     | 405 |
| ------------------------- | --- | --- | --------------------- | --- | --- |
| Ill-typed,238             |     |     | Johnston,John,374,396 |     |     |
| Implementationlanguage,57 |     |     | Joy,Bill,395          |     |     |
Implementation
Kamin,Samuel,377,396
ofADT,31,377
| ofmoduleinterface,278 |     |     | Kelsey,Richard,374,396 |     |     |
| --------------------- | --- | --- | ---------------------- | --- | --- |
Kiczales,Gregor,377,396
ofobject-orientedinterface,353,356
Kleeneplus,7,54(ex.2.29)
IMPLICIT-REFS,113–119,243(ex.7.6),
| 375 |     |     | Kleene star | (closure), 7, | 54 (ex. 2.29), |
| --- | --- | --- | ----------- | ------------- | -------------- |
381
continuation-passinginterpreterfor,
| 153(ex.5.9–10) |     |     | Knownprocedures,101(ex.3.43–44) |     |     |
| -------------- | --- | --- | ------------------------------- | --- | --- |
Knuth,Donald,371,378,396
Inclusiveor,19
Kranz,David,376,396
Inductionhypothesis,11
| Induction, | proof by, 11–12, | 25 (ex. | Krishnamurthi,Shriram,394 |     |     |
| ---------- | ---------------- | ------- | ------------------------- | --- | --- |
1.14),197,200(ex.6.2),371
Lambdacalculus,9,138,374
Inductivespecifications,1–5,371
|                             |     |     | Lambdaexpression(LcExp), |        | 9–10, 12  |
| --------------------------- | --- | --- | ------------------------ | ------ | --------- |
| recursiveproceduresbasedon, |     | 12– |                          |        |           |
|                             |     |     | (ex. 1.5),18–19,         | 42–43, | 43–44(ex. |
21
|     |     |     | 2.15–17), | 50 (ex. | 2.23), 54 (ex. |
| --- | --- | --- | --------- | ------- | -------------- |
INFERRED,248–270,271,272
2.27–30)
Ingerman,Peter,375,396
abstractvs.concretesyntax,51–53
Inheritance,326,329–334
Schemeimplementation,46–50
| Inheritedattribute,23–24,371 |                |               | Lam,Monica,393 |     |     |
| ---------------------------- | -------------- | ------------- | -------------- | --- | --- |
| Inlining,                    | 22 (ex. 1.12), | 195, 199, 201 |                |     |     |
Landin,Peter,376,396
(ex.6.4,6.7)
Languageprocessors,57–59
in-S?,1
Lazyevaluation,136–138,375
| Instanceofclass,326       |     |     | Leroy,Xavier,377,396              |          |           |
| ------------------------- | --- | --- | --------------------------------- | -------- | --------- |
| Instancevariables,325,326 |     |     | LET,60–70                         |          |           |
| Interface                 |     |     | letcc expression,                 | 178 (ex. | 5.42–44), |
| ofADT,31                  |     |     | 232,376                           |          |           |
| ofclass,353,365,369       |     |     | letmutableexpression,121(ex.4.20) |          |           |
| ofmodule,276,278          |     |     | LETREC,82–83                      |          |           |
Interfacepolymorphism,353 continuation-passinginterpreterfor,
Interpreter,ix–xii,xv,374
141–153,154–155,156
| continuation-passing,141–153,154–    |     |     | namelessversionof,91–100 |               |           |
| ------------------------------------ | --- | --- | ------------------------ | ------------- | --------- |
| 155,156,201(ex.6.7)                  |     |     | let*scoping,             | 74(ex. 3.17), | 278, 280, |
| InterpreterRecipe,37                 |     |     | 284,306                  |               |           |
| inttype,237                          |     |     | Lewis,Bil,376,396        |               |           |
| Invariant,10–11,327,371              |     |     | Lexical addressing,      | 91–93,        | 349 (ex.  |
| Iterativecontrolbehavior,140,153(ex. |     |     | 9.19–20),374             |               |           |
| 5.14),193                            |     |     | Lexicaldepth,91–93       |               |           |
Jacobs,Bart,371,396
Lexicalscoperules,76–77,89,374
| Jim,Trevor,376,393 |     |     | Lexicalspecification,58,379 |     |     |
| ------------------ | --- | --- | --------------------------- | --- | --- |
| Johnson,Ralph,395  |     |     | Lexicalvariables,89         |     |     |

406 Index
| Liskov,Barbara,377,396 |     |     | Morris,Jr.,James,371,376,397 |     |     |
| ---------------------- | --- | --- | ---------------------------- | --- | --- |
listexpression,73(ex.3.10),108(ex. Multiple-argumentprocedures,80–81
4.5), 112 (ex. 4.11), 153 (ex. 5.6), (ex. 3.20–21),83(ex. 3.31),85(ex.
| 221(ex.6.24)      |     |     | 3.33),113(ex.4.13),121(ex.4.17), |                |             |
| ----------------- | --- | --- | -------------------------------- | -------------- | ----------- |
| list-length,13–14 |     |     | 153 (ex.                         | 5.8), 166 (ex. | 5.25), 203, |
Listofintegers(List-of-Int),4,6–7,28– 243 (ex. 7.5), 270 (ex. 7.24), 289
| 29(ex.1.28–30)                   |                |             | (ex. 8.4),              | 310 (ex. 8.16), | 323 (ex.  |
| -------------------------------- | -------------- | ----------- | ----------------------- | --------------- | --------- |
| Listofsymbols(List-of-Symbol),15 |                |             | 8.25),334               |                 |           |
| List operations,                 | 73 (ex. 3.9),  | 153 (ex.    | Multipleinheritance,329 |                 |           |
| 5.5), 247                        | (ex. 7.9), 271 | (ex. 7.25), | Multiple-procedure      | declaration,    | 84–       |
| 334                              |                |             | 85(ex. 3.32–33),        | 87(ex.          | 3.36–37), |
Lists(List),13–18,27–29(ex.1.15–27) 121 (ex. 4.18–19), 203, 243 (ex.
list-sum, 24, 201 (ex. 6.4), 203 (ex. 7.5), 259, 270 (ex. 7.24), 289 (ex.
| 6.10)         |     |     | 8.4),310(ex.8.16),334 |              |         |
| ------------- | --- | --- | --------------------- | ------------ | ------- |
| Locations,103 |     |     | Multiple-variable     | declaration, | 74 (ex. |
Łukasiewicz,Jan,55(ex.2.31) 3.16),153(ex. 5.7),221(ex. 6.25),
L-values,104,375.SeealsoReferences 243 (ex. 7.5), 270 (ex. 7.24), 289
(ex.8.4),310(ex.8.16),334
Machinelanguage,58,374
Multithreadedprograms,179–189,376
| MacQueen,David,397            |     |           | MUTABLE-PAIRS,               | 124–128, | 129, 248 |
| ----------------------------- | --- | --------- | ---------------------------- | -------- | -------- |
| McCarthy,John,371,374,375,397 |     |           | (ex.7.11)                    |          |          |
| Member function.              | See | Method of |                              |          |          |
|                               |     |           | Mutablevariables,116,121(ex. |          | 4.16),   |
| object                        |     |           | 375                          |          |          |
Memberofobject,325,326,340–342 Mutation, 104, 109, 113, 229–231. See
Memoization,138,375
alsoAssignment
| Messagepassing,object-oriented(method |     |     | Mutex,187–189,190 |     |     |
| ------------------------------------- | --- | --- | ----------------- | --- | --- |
calls),325,327,335–336,337,340–
|                         |     |     | Mutual recursion,     | 20–21,         | 48, 81 (ex. |
| ----------------------- | --- | --- | --------------------- | -------------- | ----------- |
| 342,359–362             |     |     | 3.24), 84–85          | (ex. 3.32-33), | 87 (ex.     |
| Metacircularity,374,375 |     |     | 3.36–37),124(ex.4.26) |                |             |
Methodenvironments,345
Myhrhaug,Bjorn,393
Methodofobject,325,326–327
| declarationof,334,365,368       |     |     | Namelessenvironment,98–99 |     |     |
| ------------------------------- | --- | --- | ------------------------- | --- | --- |
| overloadingof,349(ex.9.16,9.22) |     |     | Namemangling,349(ex.9.22) |     |     |
| Michie,Donald,375,397           |     |     | Names,eliminating,374     |     |     |
| Milne,Robert,375,397            |     |     | fromLETREC,91–100         |     |     |
Milner,Robin, 274, 374, 375, 376–377, fromCLASSES,349(ex.9.19–20)
| 397                      |     |     | NaturalnumbersADT,32–33        |     |       |
| ------------------------ | --- | --- | ------------------------------ | --- | ----- |
| Moduleprocedures,311–323 |     |     | withbignumrepresentation,33    |     |       |
| Modules,275–276,326,377  |     |     | with diff-treerepresentation,  |     | 34–35 |
| parameterized,311–323    |     |     | (ex.2.3)                       |     |       |
| simple,276–289           |     |     | moduleimplementationof,316–317 |     |       |
| Moggi,Eugenio,375,397    |     |     | (ex.8.20–22)                   |     |       |
| Monads,375               |     |     | withSchemenumbers,33           |     |       |
| Montanari,Ugo,395        |     |     | withunaryrepresentation,33     |     |       |

| Index                       |     |     |                             |     | 407 |
| --------------------------- | --- | --- | --------------------------- | --- | --- |
| Naur,Peter,375,397          |     |     | Philbin,James,396           |     |     |
| vonNeumann,John,374,375,399 |     |     | Pierce,Benjamin,377,396,397 |     |     |
| Nielsen,Lasse,376,394       |     |     | Pizza,191                   |     |     |
NoMysteriousAuxiliaries,23,197 Plotkin, Gordon, 371, 374, 375, 376,
| Nonstandard     | control flow, | 171–177, | 398                              |     |     |
| --------------- | ------------- | -------- | -------------------------------- | --- | --- |
| 202(ex.6.8),232 |               |          | Polishprefixnotation,55(ex.2.31) |     |     |
Nonterminal symbols, 6, 20–21, 22, Polymorphism,237,255,266,269–270,
| 51–52,382                          |     |     | 273–274                   | (ex. 7.28–30), | 329, 353, |
| ---------------------------------- | --- | --- | ------------------------- | -------------- | --------- |
| No-occurrenceinvariant,258,262–263 |     |     | 377                       |                |           |
| Notype,238                         |     |     | Pratt,Terrence,398        |                |           |
| nth-element,14–16                  |     |     | Predicates,42–43          |                |           |
| number-elements,22–23,30(ex.1.36)  |     |     | Pre-emptivescheduling,179 |                |           |
Prefixlists(Prefix-list),55(ex.2.31)
Objects,325,339
Printing,74(ex.3.15),227–229
Observers,33
Privatevariables,105
| Occurrencecheck,258 |     |     | PROC,75–80 |     |     |
| ------------------- | --- | --- | ---------- | --- | --- |
occurs-free?,18–19,43,46–47,201(ex.
Proceduralrepresentation,377
6.4)
|     |     |     | of continuations, | 146, | 147, 153 (ex. |
| --- | --- | --- | ----------------- | ---- | ------------- |
Opaquetype,34,292,296–300,326
|     |     |     | 5.1), 178 | (ex. 5.41), | 194, 198, 201 |
| --- | --- | --- | --------- | ----------- | ------------- |
OPAQUE-TYPES,292–311
(ex.6.4,6.7)
Operandposition,140,206
ofenvironments,40–41,42(ex.2.12–
| Operands, | 75, 136–138, | 141, 152–153, |     |     |     |
| --------- | ------------ | ------------- | --- | --- | --- |
14),85(ex.3.34)
203,215–220
ofprocedurevalues,79,82(ex.3.28)
| Operator,75 |     |     | ofstacks,42(ex.2.12) |     |     |
| ----------- | --- | --- | -------------------- | --- | --- |
Or,7,19
oftrampolining,159
Orwant,Jon,399
|                             |     |       | Procedure | call, 75–77, | 151–152, 217– |
| --------------------------- | --- | ----- | --------- | ------------ | ------------- |
| Overloadingofmethod,349(ex. |     | 9.16, |           |              |               |
220,226
9.22)
|     |     |     | Procedure | declaration, | 75–77, 80 (ex. |
| --- | --- | --- | --------- | ------------ | -------------- |
Overridenmethod,331,365
3.19),101(ex.3.43–44),214
| Pairtypes,243(ex.7.8) |     |     | Proceduretypes,237,240,241–242 |             |           |
| --------------------- | --- | --- | ------------------------------ | ----------- | --------- |
|                       |     |     | for module                     | procedures, | 318, 319– |
Parameterizedmodules,311–323
323
| Parameter | passing, 76, 118, | 119, 335– |     |     |     |
| --------- | ----------------- | --------- | --- | --- | --- |
Procedurevalues(Proc),75–77
336
datastructurerepresentationof,79–
Pardo,L.,378,396
|     |     |     | 80,81(ex. | 3.26),82(ex. | 3.28),101 |
| --- | --- | --- | --------- | ------------ | --------- |
Parentclass,329
(ex.3.42),225(ex.6.32)
| Parnas,David,377,397 |     |     | proceduralrepresentationof,79,82 |     |     |
| -------------------- | --- | --- | -------------------------------- | --- | --- |
parse-expression,53,54(ex.2.29–30)
(ex.3.28)
| Parsergenerator,53,58–59 |     |     | PROC-MODULES,311–323 |     |     |
| ------------------------ | --- | --- | -------------------- | --- | --- |
Parsing,53, 58–59, 371, 382–383, 385– Productionofgrammar,7,51–52,385
| 391                      |     |     | Protectioninobject-orientedprogram- |     |     |
| ------------------------ | --- | --- | ----------------------------------- | --- | --- |
| partial-vector-sum,24–25 |     |     | ming,348(ex.9.11–13)                |     |     |
Paulson,Laurence,376,377,397
Prototypeobjects,352(ex.9.29)
| PeytonJones,Simon,374,375,397 |     |     | Pseudo-variable,336,340 |     |     |
| ----------------------------- | --- | --- | ----------------------- | --- | --- |

408 Index
| Qualifiedtype,295,302     |     |     |     | desRivières,Jim,396     |
| ------------------------- | --- | --- | --- | ----------------------- |
| Qualifiedvariable,278,283 |     |     |     | Robson,David,377,395    |
| Quantum,179               |     |     |     | vanRossum,Guido,378,399 |
Rulesofinference,3,65
readstatement,123(ex.4.23) Rules-of-inferencedefinition,3,4,371
Record,48
Rutten,Jan,371,396
| Recursive | control | behavior, | 140, 153 | R-values,104,375 |
| --------- | ------- | --------- | -------- | ---------------- |
(ex.5.14)
| Recursivedatatypes.SeealsoAbstract |      |             |        | Sabry,Amr,376,394,398            |
| ---------------------------------- | ---- | ----------- | ------ | -------------------------------- |
| datatypes                          |      |             |        | Safeevaluation                   |
| programs                           | that | manipulate, | 12–13, | threadsynchronization,186–189    |
| 22–25,45–50                        |      |             |        | typesafety,233–235,358           |
| provingpropertiesof,11–12,24–25    |      |             |        | Scanning,58,379–382,383–385      |
| specifying,1–11,43                 |      |             |        | Schaffert,Craig,396              |
| Recursiveprograms                  |      |             |        | Scheduler,179,183,184            |
| deriving,12–13,22                  |      |             |        | Scopeofvariabledeclaration,87–91 |
design and implementation of, 81 dynamic, 82 (ex. 3.28–29), 87 (ex.
| (ex. 3.25),            | 87               | (ex.           | 3.37), 121 (ex. | 3.37),168(ex.5.30)                  |
| ---------------------- | ---------------- | -------------- | --------------- | ----------------------------------- |
| 4.16),371,374          |                  |                |                 | Scott,Michael,398                   |
| examplesof,12–21,25–31 |                  |                |                 | Sebesta,Robert,398                  |
| mutualrecursion,20–21, |                  |                | 48,81(ex.       | self,328–329,335,336,342,359        |
| 3.24), 84–85           |                  | (ex. 3.32-33), | 87 (ex.         | Semaphore,187–189,190               |
| 3.36–37),124(ex.4.26)  |                  |                |                 | Semi-infiniteextent,90,168(ex.5.30) |
| Red-blue trees         | (Red-blue-tree), |                | 29 (ex.         | Separatedlistnotation,7–8           |
| 1.33),51(ex.2.26)      |                  |                |                 | Sequences,bidirectional,44(ex.2.18) |
Rees,Jonathan,374,394,396,398 Sequentialization,201(ex. 6.4–5),221
| References,87–91,103–104 |     |     |     | (ex.6.20),226(ex.6.34–35) |
| ------------------------ | --- | --- | --- | ------------------------- |
setdynamicexpression,122(ex.4.21)
| explicit,                | 104–111, | 229–231, | 248 (ex.  |                           |
| ------------------------ | -------- | -------- | --------- | ------------------------- |
| 7.10)                    |          |          |           | Sethi,Ravi,393            |
| implicit,113–119,231(ex. |          |          | 6.37),243 | S-exp(S-exp),8–9,20–21,48 |
| (ex.7.6)                 |          |          |           | Shadowing,89,330          |
Registerization,160–166,167,168,169, Shared variables, 103, 104, 106, 122
| 170, 189              | (ex. | 5.50), | 194, 195, 201   | (ex.4.21),160,179,186–189 |
| --------------------- | ---- | ------ | --------------- | ------------------------- |
| (ex. 6.4),            | 212  | (ex.   | 6.16), 225 (ex. | Simpleexpressions,206,226 |
| 6.33)                 |      |        |                 | SIMPLE-MODULES,276–289    |
| Regularexpression,380 |      |        |                 | Singleinheritance,329     |
remove-first,16–18,201(ex.6.4) S-list(S-list), 8–9, 20–21, 27 (ex. 1.18,
| report-procedures,15 |     |     |     | 1.20),28(ex.1.27),48 |
| -------------------- | --- | --- | --- | -------------------- |
Representationindependence,32,35 Smaller-SubproblemPrinciple,12–13
| Reynolds,John,375,377,398 |     |     |              | Smallestset,3,6(ex.1.3)  |
| ------------------------- | --- | --- | ------------ | ------------------------ |
| Ribcage representation,   |     |     | 40, 101 (ex. | Smith,Joshua,376,377,398 |
| 3.41)                     |     |     |              | Snyder,Alan,396          |

| Index                   |     |     |                                  |     | 409 |
| ----------------------- | --- | --- | -------------------------------- | --- | --- |
| Soundtypesystem,234     |     |     | Tail-formexpressions,203–207,375 |     |     |
| Sourcelanguage,57       |     |     | Tailposition,205–206             |     |     |
| Sperber,Michael,374,398 |     |     | Targetlanguage,58                |     |     |
Springer,George,377,398
Terminalsymbols,6
| Stacks, 37 | (ex. 2.4), 42 (ex. | 2.12), 50 |     |     |     |
| ---------- | ------------------ | --------- | --- | --- | --- |
Thatcher,James,395
(ex.2.22)
Thaw,136
State. SeeEXPLICIT-REFS;IMPLICIT-
Thomas,Dave,378,39
REFS
Threads,179–189,376
| Statements,122–124(ex. |     | 4.22–27),155 |                   |          |           |
| ---------------------- | --- | ------------ | ----------------- | -------- | --------- |
|                        |     |              | throw expression, | 178 (ex. | 5.42–44), |
(ex.5.16)
232
Staticenvironment,94–96
Thunk,136–137,375
| Static method                   | dispatch, | 333, 350 (ex. |               |     |     |
| ------------------------------- | --------- | ------------- | ------------- | --- | --- |
| 9.23),368(ex.9.37),371(ex.9.42) |           |               | Timeslice,179 |     |     |
Tofte,Mads,397
Staticpropertiesofprograms,88,91
| Staticvariables,348–349(ex.9.15)   |     |     | Tokens,58,379,381–382      |               |          |
| ---------------------------------- | --- | --- | -------------------------- | ------------- | -------- |
| Steele,Guy,374,376,377,395,398,399 |     |     | Top-downdefinition,2,3,371 |               |          |
|                                    |     |     | Trampolining,              | 155, 157–159, | 166 (ex. |
Storablevalues,103,106
| Store,103,109–112,229–231,375 |     |     | 5.26),194,196,212(ex.6.17) |     |     |
| ----------------------------- | --- | --- | -------------------------- | --- | --- |
datastructurerepresentationof,160
Store-passingspecifications,107
| Stoy,Joseph,374,399 |     |     | (ex.5.18–20) |     |     |
| ------------------- | --- | --- | ------------ | --- | --- |
proceduralrepresentationof,159
vanStraaten,Anton,398
Translation
Strachey,Christopher,375,397,399
| Subclass,329 |     |     | toCPS,212–220,222–224 |                     |     |
| ------------ | --- | --- | --------------------- | ------------------- | --- |
|              |     |     | nameful               | to nameless LETREC, | 93– |
Subclasspolymorphism,329,353,361
96,97,101(ex.3.40–42)
Subroutines,124
Transparenttype,34,292,294–295
subst,20–21,201(ex.6.4)
Turing,Alan,374,399
Substitution
| ins-lists,20–21 |     |     | Typeabbreviations,34,292,294–295 |     |     |
| --------------- | --- | --- | -------------------------------- | --- | --- |
Typechecking
type,252,253,258,260–262
|     |     |     | for expressions, | 240–243, | 244, 245, |
| --- | --- | --- | ---------------- | -------- | --------- |
Subtypepolymorphism,361
| Supercalls,332–334,336,337 |     |     | 246 |     |     |
| -------------------------- | --- | --- | --- | --- | --- |
formoduleprocedures,319–323
Superclass,329,331
formodules,284–289,303–310,311
| Sussman, | Gerald Jay, 374, | 375, 376, |                  |          |           |
| -------- | ---------------- | --------- | ---------------- | -------- | --------- |
|          |                  |           | object-oriented, | 352–367, | 368, 369, |
377,393,399
370
Synchronization,186–189
| Syntactic categories, | 6, 20–21, | 22, 51– | Typedefinition,302 |     |     |
| --------------------- | --------- | ------- | ------------------ | --- | --- |
TYPED-OO,352–357
52,382
Typeenvironment,239
Syntacticderivation,8
Typeequations,249–250,377
| Tables,301(ex.8.15),317(ex.8.23) |     |     | solving,252,258,272(ex.7.27) |     |     |
| -------------------------------- | --- | --- | ---------------------------- | --- | --- |
Typeexpression,260
Tailcalls,140,144,162,193,205
| Tail Calls | Don’t Grow the | Continua- | Typeinference,376–377 |     |     |
| ---------- | -------------- | --------- | --------------------- | --- | --- |
examplesof,157–158,250–258
tion,144,146,205

410 Index
| for expressions,              | 240, | 248–250, 266– | trampolinedversion,157–15     |     |
| ----------------------------- | ---- | ------------- | ----------------------------- | --- |
| 270,271,272                   |      |               | forTYPED-OO,356–357           |     |
| formodules,291(ex.8.11)       |      |               | Valuerestriction,274(ex.7.30) |     |
| Typestructure                 |      |               | Variable(s)                   |     |
| ofbasicvalues,235–237,238–240 |      |               | aliasingof,133                |     |
ofmoduleinterfaces,287–288
bindingof(seeBinding)
ofmoduleprocedures,319–323
bound,10,75
| ofobjectsandclasses,353        |     |     | class,348–349(ex.9.15) |     |
| ------------------------------ | --- | --- | ---------------------- | --- |
| opaqueandtransparenttypes,303– |     |     | declarations,87–91     |     |
| 310,311                        |     |     | eliminating,91–100     |     |
| Typevariable,250               |     |     | inenvironments,35,36   |     |
extentof,90,168(ex.5.30)
| Ullman,Jeffrey,376,377,393,399   |     |     | free,18    |     |
| -------------------------------- | --- | --- | ---------- | --- |
| Unaryrepresentationofnaturalnum- |     |     | lexical,89 |     |
mutable,116,121(ex.4.16)
bers,33
private,105
Unification,252,262–263
| unpackexpression,74(ex. |     | 3.18),101 | qualified,278              |      |
| ----------------------- | --- | --------- | -------------------------- | ---- |
|                         |     |           | scopeof(seeScopeofvariable | dec- |
(ex.3.39)
| unparse-lc-expression,53,54(ex.2.28) |     |     | laration) |     |
| ------------------------------------ | --- | --- | --------- | --- |
shadowingof,89
| value-of                           |     |               | shared,103,104,106,122(ex. | 4.21), |
| ---------------------------------- | --- | ------------- | -------------------------- | ------ |
| forCALL-BY-NAME,137                |     |               | 160,179,186–189            |        |
| forCALL-BY-NEED,137–138            |     |               | static,348–349(ex.9.15)    |        |
| forCALL-BY-REFERENCE,132–133       |     |               | typevariable,250           |        |
| forCLASSES,336–346                 |     |               | Variant,47                 |        |
| continuation-passingversionof,141– |     |               | vector-sum,24–25           |        |
| 153,154–155,156,201(ex.6.7)        |     |               | Virtualmachine,58          |        |
| forCPS-OUT,207,209                 |     |               | Vlissides,John,395         |        |
| for EXPLICIT-REFS,                 |     | 107–109, 112– |                            |        |
Wadler,Philip,375,376,396,398,399
113(ex.4.12)
Wadsworth,Christopher,375,399
forIMPLICIT-REFS,117–118
Wagner,Eric,395
forLET,62–63,65–67,70,71–72
Wall,Larry,378,399
forLETREC,83,142,142
Wand,Mitchell,376,377,394,395,399,
forMUTABLE-PAIRS,124,126
400
namelessversion,99–100
Wegbreit,Ben,371,397
forOPAQUE-TYPES,302–303
Well-typed,238
forPROC,76–77
Wexelblatt,R.,378,400
forPROC-MODULES,319,320
Winskel,Glynn,375,400
| registerized | version, | 160–166, 167, |     |     |
| ------------ | -------- | ------------- | --- | --- |
Wise,David,375,395
168,169,170
Wright,Jesse,395
forSIMPLE-MODULES,282–284,285
Wulf,William,375,400
| forTHREADS,182,185–186 |     |     | Zelkowitz,Marvin,398 |     |
| ---------------------- | --- | --- | -------------------- | --- |