|             |     | The Implementation |                       | of       | Lua | 5.0         |        |
| ----------- | --- | ------------------ | --------------------- | -------- | --- | ----------- | ------ |
|             |     |                    | Roberto Ierusalimschy |          |     |             |        |
| (Department |     | of Computer        | Science,              | PUC-Rio, | Rio | de Janeiro, | Brazil |
roberto@inf.puc-rio.br)
|                 |     | Luiz                  | Henrique de | Figueiredo  |     |             |        |
| --------------- | --- | --------------------- | ----------- | ----------- | --- | ----------- | ------ |
| (IMPA{Instituto |     | de Matema(cid:19)tica | Pura        | e Aplicada, | Rio | de Janeiro, | Brazil |
lhf@impa.br)
|             |     |             | Waldemar | Celes    |     |             |        |
| ----------- | --- | ----------- | -------- | -------- | --- | ----------- | ------ |
| (Department |     | of Computer | Science, | PUC-Rio, | Rio | de Janeiro, | Brazil |
celes@inf.puc-rio.br)
Abstract: WediscussthemainnoveltiesoftheimplementationofLua5.0:itsregister-
based virtual machine, the new algorithm for optimizing tables used as arrays, the
| implementation |            | of closures, andthe | addition       | of coroutines. |           |            |     |
| -------------- | ---------- | ------------------- | -------------- | -------------- | --------- | ---------- | --- |
| Key Words:     | compilers, | virtual             | machines, hash | tables,        | closures, | coroutines |     |
| Category:      | D.3.4,     | D.3.3, D.3.2,       | E.2            |                |           |            |     |
1 Introduction
Luawasborninanacademiclaboratoryasatoolforin-housesoftwaredevelop-
ment but somehowwasadopted by severalindustrial projectsaround the world
industry.1
| and is now | widely | used in | the game |     |     |     |     |
| ---------- | ------ | ------- | -------- | --- | --- | --- | --- |
How do we account for this widespread use of Lua? We believe that the
answer lies in our design and implementation goals: to provide an embeddable
scriptinglanguagethatissimple,e(cid:14)cient,portable,andlightweight.Thesehave
beenourmaingoalssincethebirthofLuain1993andtheyhavebeenrespected
during its evolution. (For a history of the development of Lua up to just before
the release of Lua5.0, see[12].) These features, plus the fact that Lua has been
designed from the start to be embedded into largerapplications, account for its
industry.2
| early adoption |     | by the |     |     |     |     |     |
| -------------- | --- | ------ | --- | --- | --- | --- | --- |
Widespread use generates demand for language features. Several features
of Lua have been motivated by industrial needs and user feedback. Important
examples are the introduction of coroutines in Lua5.0 and the implementation
1 An informal poll conducted in September 2003 by gamedev.net, an important site
forgameprogrammers,showedLuaasthemostpopularscriptinglanguageforgame
development.Fordetails,seehttp://www.gamedev.net/gdpolls/viewpoll.asp?ID=
163.
2
| The adoption |     | of a liberal MIT-like | license | also helped. |     |     |     |
| ------------ | --- | --------------------- | ------- | ------------ | --- | --- | --- |

of incremental garbage collection in the upcoming Lua5.1. Both features are
specially important for games.
Inthispaper,wediscussthemainnoveltiesoftheimplementationofLua5.0,
compared to Lua4.0:
Register-based virtual machine: Traditionally, most virtual machines intended
for actual execution are stack based, a trend that started with Pascal’s P-
machine[15]and continuestodaywith Java’sJVM andMicrosoft’s.Net en-
vironment.Currently,however,therehasbeenagrowinginterestinregister-
based virtual machines (for instance, the planned new virtual machine for
Perl6 (Parrot) will be register based[17]). As far as we know, the virtual
machineof Lua5.0isthe (cid:12)rstregister-basedvirtualmachineto haveawide
use. This virtual machine is presented in Section7.
New algorithm for optimizing tables used as arrays: Unlike other scripting lan-
guages, Lua does not o(cid:11)er an array type. Instead, Lua programmers use
regular tables with integer indices to implement arrays. Lua5.0 uses a new
algorithm that detects whether tables are being used as arrays and auto-
matically stores the values associated to numeric indices in an actual array,
instead of adding them to the hash table. This algorithm is discussed in
Section4.
The implementation of closures:Lua5.0supports(cid:12)rst-classfunctionswithlex-
ical scoping. This mechanism poses a well-known di(cid:14)culty for languages
that use an array-based stack to store activation records. Lua uses a novel
approachtofunction closuresthat keepslocalvariablesin the (array-based)
stack and only moves them to the heap if they go out of scope while being
referred by nested functions. The implementation of closures is discussed in
Section5.
The addition of coroutines: Lua5.0 introduced coroutines in the language.
Although the implementation of coroutines is more or less traditional, we
present a short overview in Section6 for completeness.
The other sections complement or give background to this discussion. In Sec-
tion2 we present an overview of Lua’s design goals and how those goals have
driven its implementation. In Section3 we describe how Lua represents its val-
ues. Although the representation itself has no novelties, we need this material
for the other sections. Finally, in Section8 we present a small benchmark and
draw some conclusions.
2 An Overview of Lua’s Design and Implementation
As mentioned in the introduction, the goals in our implementation of Lua are:

Simplicity:Weseekthesimplestlanguagewecana(cid:11)ordandthesimplestCcode
that implements this language. This implies a simple syntax with a small
number of language constructs, not far from the tradition.
E(cid:14)ciency: We seek fast compilation and fast execution of Lua programs.This
implies a fast, smart, one-pass compiler and a fast virtual machine.
Portability: WewantLuatorunonasmanyplatformsaspossible.We wantto
be abletocompilethe Luacoreunmodi(cid:12)edeverywhereandtorunLuapro-
gramsunmodi(cid:12)edoneveryplatformthathasasuitableLuainterpreter.This
impliesacleanANSICimplementationwith specialattentiontoportability
issues,suchasavoidingdarkcornersof Canditslibraries,andensuringthat
it also compiles cleanly asC++. We seek warning-freecompilations.
Embeddability: Luaisanextensionlanguage;itisdesignedtoprovidescripting
facilities to larger programs. This and the other goals imply the existence
of a CAPI that is simple and powerful, but which relies mostly on built-in
Ctypes.
Lowembeddingcost:WewantittobeeasytoaddLuatoanapplicationwithout
bloating it. This implies tight Ccode and a small Lua core, with extensions
being added as user libraries.
These goals are somewhat con(cid:13)icting. For instance, Lua is frequently used as a
data-description language, for storing and loading con(cid:12)guration (cid:12)les and some-
timesquitelargedatabases(Luaprogramswithafewmegabytesarenotuncom-
mon).ThisimpliesthatweneedafastLuacompiler.Ontheotherhand,wewant
Luaprogramstoexecutefast.Thisimpliesasmartcompiler,onethatgenerates
good code for the virtual machine. So, the implementation of the Lua compiler
has to balance between these two requirements. However, the compiler cannot
betoolarge;otherwiseitwouldbloatthewholepackage.Currentlythecompiler
accountsforapproximately30%of the sizeof the Luacore.Formemory-limited
applications,suchasembeddedsystems,itispossibletoembedLuawithoutthe
compiler.Luaprogramsarethen precompiledo(cid:11)-lineandloadedatruntime by
a tiny module (which is also fast because it loads binary (cid:12)les).
Luausesahand-writtenscannerandahand-writtenrecursivedescentparser.
Untilversion3.0,Luausedaparserproducedbyyacc[13],whichprovedavalu-
abletoolwhenthelanguage’ssyntaxwaslessstable.However,thehand-written
parser is smaller, more e(cid:14)cient, more portable, and fully reentrant. It also pro-
vides better error messages.
The Lua compiler uses no intermediate representation. It emits instructions
forthevirtualmachine\onthe(cid:13)y"asitparsesaprogram.Nevertheless,itdoes
perform some optimizations. For instance, it delays the generation of code for
baseexpressionslikevariablesandconstants.Whenitparsessuchexpressions,it

generates no code; instead, it uses a simple structure to represent them. There-
fore, it is very easy to check whether an operand for a given instruction is a
constantoralocalvariableandusethosevaluesdirectlyinthe instruction,thus
avoiding unnecessary and costly moves (see Section3).
To be portable across many di(cid:11)erent Ccompilers and platforms, Lua can-
not use several tricks commonly used by interpreters, such as direct threaded
code[8, 16]. Instead, it uses a standard while{switch dispatch loop. Also, at
placestheCcodeseemsundulycomplicated,butthecomplicationistheretoen-
sure portability. The portability of Lua’s implementation has increasedsteadily
throughout the years, as Lua got compiled under many di(cid:11)erent Ccompilers
in many di(cid:11)erent platforms (including several 64-bit platforms and some 16-bit
platforms).
Weconsiderthatwehaveachievedourdesignandimplementationgoals.Lua
is a very portable language: it runs on any machine with an ANSIC compiler,
fromembeddedsystemstomainframes.Luaisreallylightweight:forinstance,on
Linuxitsstand-aloneinterpreter,completewithallstandardlibraries,takesless
than 150Kbytes; the core is less than 100Kbytes. Lua is e(cid:14)cient: independent
benchmarks[2,4]showLuaasoneofthefastestlanguageimplementationsinthe
realmofscriptinglanguages(i.e.,interpretedanddynamically-typedlanguages).
We also consider Lua a simple language, being syntactically similar to Pascal
and semantically similar to Scheme, but this is subjective.
3 The Representation of Values
Luaisadynamically-typedlanguage:typesareattachedtovaluesratherthanto
variables.Luahaseightbasictypes: nil, boolean, number,string, table, function,
userdata, and thread. Nil is a marker type having only one value, also called
nil.Booleanvaluesaretheusualtrueandfalse.Numbersaredouble-precision
(cid:13)oating-pointnumbers,correspondingtothe typedoubleinC, but itis easyto
compile Lua using float or long instead. (Several games consoles and smaller
machines lack hardware support for double.) Strings are arrays of bytes with
an explicit size, and so can contain arbitrary binary data, including embedded
zeros. Tables are associative arrays,which can be indexed by any value (except
nil) and can hold any value. Functions are either Luafunctions or Cfunctions
written according to a protocol for interfacing with the Lua virtual machine.
Userdataareessentiallypointerstousermemoryblocks,andcomeintwo(cid:13)avors:
heavy, whose blocks are allocated by Lua and are subject to garbage collection,
and light, whose blocks are allocated and freed by the user. Finally, threads
representcoroutines.Valuesof alltypesare(cid:12)rst-classvalues: wecanstorethem
in global variables, local variables and table (cid:12)elds, pass them as arguments to
functions, return them from functions, etc.

| typedef    | struct { |     | typedef | union      | {    |
| ---------- | -------- | --- | ------- | ---------- | ---- |
| int        | t;       |     |         | GCObject   | *gc; |
| Value      | v;       |     |         | void *p;   |      |
| } TObject; |          |     |         | lua_Number | n;   |
int b;
|     |           |            | }               | Value;    |        |
| --- | --------- | ---------- | --------------- | --------- | ------ |
|     | Figure 1: | Lua values | are represented | as tagged | unions |
Lua represents values as tagged unions, that is, as pairs (t;v), where tis
an integer tag identifying the type of the valuev, which is a union of Ctypes
implementing Lua types. Nil has a single value. Booleans and numbers are im-
plemented as‘unboxed’ values: v representsvaluesof those types directly in the
union.Thisimpliesthattheunionmusthaveenoughspaceforadouble.Strings,
tables, functions, threads, and userdata values are implemented by reference:
v contains pointers to structures that implement those values. Those structures
share a common head, which keeps information needed for garbage collection.
| The rest | of the structure | is speci(cid:12)c | to eachtype. |     |     |
| -------- | ---------------- | ----------------- | ------------ | --- | --- |
Figure1showsaglimpseoftheactualimplementationofLuavalues.TObject
isthemainstructureinthisimplementation:itrepresentsthetaggedunions(t;v)
describedabove.Valueistheunionthatimplementsthevalues.Valuesoftypenil
are not explicitly represented in the union because the tag is enough to identify
them. The (cid:12)eldn is used for numbers (by default, lua_Numberis double). The
(cid:12)eldb is used for booleans.The (cid:12)eldp is used for light userdata.The (cid:12)eldgcis
usedfortheothervalues(strings,tables,functions,heavyuserdata,andthreads),
| which are | those subject | to garbagecollection. |     |     |     |
| --------- | ------------- | --------------------- | --- | --- | --- |
One consequence of using tagged unions to represent Lua values is that
copying values is a little expensive: on a 32-bit machine with 64-bit doubles,
the size of a TObject is 12bytes (or 16bytes, if doubles are aligned on 8-
byte boundaries) and so copying a value requires copying 3 (or4) machine
words. However, it is di(cid:14)cult to implement a better representation for values
in ANSI C.Severaldynamically-typedlanguages(e.g., the originalimplementa-
tion of Smalltalk80[9]) use spare bits in each pointer to store the value’s type
tag.Thistrickworksinmostmachinesbecause,duetoalignment,thelasttwoor
three bits of a pointer are alwayszero, and therefore can be used for other pur-
poses.However,thistechniqueisneitherportablenorimplementableinANSI C.
The Cstandard does not even ensures that a pointer (cid:12)ts in any integral type
and so there is no standard way to perform bit manipulation overpointers.
Another option to reduce the size of a value would be to keep the explicit
tag, but to avoid putting a double in the union. Forinstance, all numberscould

be represented as heap-allocated objects, just like strings. (Python uses this
technique, except that it preallocatessome small integer values.) However,that
representationwouldmakethelanguagequiteslow.Alternatively,integervalues
couldberepresentedasunboxedvalues,directlyinsidetheunion,while(cid:13)oating-
point values would go to the heap. That solution would greatly increase the
complexity of the implementation of all arithmetic operations.
Likeearlierinterpreted languages,such asSnobol[11] and Icon[10], Lua in-
ternalizes stringsusingahashtable:itkeepsasinglecopyofeachstringwithno
duplications.Moreover,stringsareimmutable:onceinternalized,astringcannot
be changed. Hash values for strings are computed by a simple expression that
mixes bitwise and arithmetic operations, thus shu(cid:15)ing all bits involved. Hash
values are saved when the string is internalized to allow fast string comparison
and table indexing. The hash function does not look at all bytes of the string
if the string is too long. This allows fast hashing of long strings. Avoiding loss
of performance when handling long strings is important because they are com-
mon in Lua. For instance, it is usual to process (cid:12)les in Lua by reading them
completely into memory into a single long string.
4 Tables
Tables are the main | in fact, the only | data-structuring mechanism in Lua.
Tables play a key role not only in the language but also in its implementation.
E(cid:11)ort spent on a good implementation of tables is rewarded in the language
because tables are used for several internal tasks, with no qualms about perfor-
mance. Thishelps to keepthe implementationsmall. Conversely,the absenceof
any other data-structuring mechanism places a pressure on an e(cid:14)cient imple-
mentation of tables.
Tables in Lua are associative arrays, that is, they can be indexed by any
value(except nil)andcanholdvaluesof anytype.Moreover,theyaredynamic
in the sense that they may grow when data is added to them (by assigning a
value to a hitherto non-existent (cid:12)eld) and shrink when data is removed from
them (by assigning nil to a (cid:12)eld).
Unlike many other scripting languages, Lua does not have an array type.
Arrays are represented as tables with integer keys. The use of tables for ar-
rays bring bene(cid:12)ts to the language. The main one is simplicity: Lua does not
need two di(cid:11)erent sets of operators to manipulate tables and arrays. Moreover,
programmers do not have to choose between the two representations. The im-
plementationofsparsearraysistrivialinLua.InPerl,forinstance,youcanrun
outof memoryif youtrytorunthe program$a[1000000000]=1;,asit triggers
the creationof anarraywith onebillionelements. Theequivalent Luaprogram,
a={[1000000000]=1},creates a table with a single entry.

value
key value
100
"x" 9.2
200
nil
300
Header
nil
Figure 2: A Lua table.
UntilLua4.0,tableswereimplemented strictlyashashtables:allpairswere
explicitly stored. Lua5.0 brought a new algorithm to optimize the use of tables
asarrays:itoptimizespairswithintegerkeysbynotstoringthekeysandstoring
the valuesinanactualarray.Moreprecisely,inLua5.0,tablesareimplemented
as hybrid data structures: they contain a hash part and an arraypart. Figure2
shows a possible con(cid:12)guration for a table with the pairs "x"!9:3, 1!100,
2!200,3!300.Notethe arraypartonthe right:itdoesnotstoretheinteger
keys. This division is made only at a low implementation level; access to table
(cid:12)elds is transparent, even to the virtual machine. Tables automatically and dy-
namicallyadapt their twoparts accordingto their contents:the arrayparttries
to store the values correspondingto integerkeys from1 to some limitn. Values
correspondingto non-integerkeysor to integerkeys outside the arrayrangeare
stored in the hash part.
When a table needs to grow, Lua recomputes the sizes for its hash part and
its array part. Either part may be empty. The computed size of the array part
is the largest n such that at least half the slots between 1 and n are in use
(to avoid wasting space with sparse arrays) and there is at least one used slot
between n=2+1 and n (to avoid a sizen when n=2would do). After computing
the new sizes, Lua creates the new parts and re-inserts the elements from the
old parts into the new ones. As an example, suppose that a is an empty table;
bothitsarraypartandhashparthavesizezero.Ifweexecutea[1]=v,the table
needs to grow to accommodate the new key. Lua will choose n = 1 for the size
of the new array part (with a single entry 1 ! v). The hash part will remain
empty.
This hybrid scheme has two advantages. First, access to values with integer
keys is faster because no hashing is needed. Second, and more important, the
array part takes roughly half the memory it would take if it were stored in the

hashpart,becausethekeysareimplicitinthearraypartbutexplicitinthehash
part. As a consequence, if a table is being used as an array, it performs as an
array,aslongasitsintegerkeysaredense.Moreover,nomemoryortimepenalty
is paid for the hash part, because it does not even exist. The converse holds: if
the table is being used as an associative array, and not as an array, then the
array part is likely to be empty. These memory savings are important because
it is common for a Lua program to create many small tables, for instance when
| tables are | used to | implement | objects. |     |     |
| ---------- | ------- | --------- | -------- | --- | --- |
The hash part uses a mix of chained scatter table with Brent’s variation[3].
A main invariant of these tables is that if an element is not in its main position
(i.e., the original position given by its hash value), then the colliding element
is in its own main position. In other words, there are collisions only when two
elements have the same main position (i.e., the same hash values for that table
size).Therearenosecondarycollisions.Becauseofthat,the loadfactorofthese
| tables can  | be 100% | without      | performance | penalties. |     |
| ----------- | ------- | ------------ | ----------- | ---------- | --- |
| 5 Functions |         | and Closures |             |            |     |
When Lua compiles a function it generates a prototype containing the vir-
tual machine instructions for the function, its constant values (numbers, literal
strings,etc.), andsomedebuginformation.Atruntime, wheneverLuaexecutes
a function...endexpression, it creates a new closure. Each closure has a ref-
erence to its corresponding prototype, a reference to its environment (a table
wherein it looks for global variables), and an array of references to upvalues,
| which are | used to | access | outer local | variables. |     |
| --------- | ------- | ------ | ----------- | ---------- | --- |
The combination of lexical scoping with (cid:12)rst-class functions creates a well-
known di(cid:14)culty for accessing outer local variables. Consider the example in
Figure3.Whenadd2iscalled,itsbodyaccessestheouterlocalvariablex(func-
tionparametersinLuaarelocalvariables).However,bythe timeadd2iscalled,
the functionaddthatcreatedadd2hasalreadyreturned.If xwascreatedinthe
| stack, its | stack slot | would | no longer | exist.         |          |
| ---------- | ---------- | ----- | --------- | -------------- | -------- |
| function   | add (x)    |       |           | add2           | = add(2) |
| return     | function   | (y)   |           | print(add2(5)) |          |
return x+y
end
end
|     |     | Figure | 3: Access | to outer local | variables |
| --- | --- | ------ | --------- | -------------- | --------- |

pending vars.
pending vars.
top
|     |     | (open)  |     |     | (closed) |
| --- | --- | ------- | --- | --- | -------- |
|     |     | upvalue |     |     | upvalue  |
x
x
top
| stack |                  |       |                | stack           |                  |
| ----- | ---------------- | ----- | -------------- | --------------- | ---------------- |
|       |                  | x     |                |                 | x                |
|       | function closure |       |                |                 | function closure |
|       | Figure           | 4: An | upvalue before | and after being | \closed".        |
Most procedural languages avoid this problem by restricting lexical scoping
(e.g.,Python),notproviding(cid:12)rst-classfunctions(e.g.,Pascal),orboth(e.g., C).
Functional languagesdo not havethose restrictions.Researchin non-pure func-
tionallanguageslikeSchemeandMLhascreatedavastbodyofknowledgeabout
compilation techniques for closures (e.g.,[19, 1, 21]).3 However, those works do
not try to limit the complexity of the compiler. For instance, just the control-
(cid:13)ow analysis of Bigloo, an optimizer Scheme compiler[20], is more than ten
times larger than the whole Lua implementation: The source for module Cfa
of Bigloo2.6f has 106,350 lines, versus 10,155 lines for the core of Lua5.0. As
| explained | in Section2, | Lua | needs something | simpler. |     |
| --------- | ------------ | --- | --------------- | -------- | --- |
Luausesastructurecalledanupvalue toimplementclosures.Anyouterlocal
variableisaccessedindirectlythroughanupvalue.Theupvalue originallypoints
to the stack slot wherein the variable lives (Figure4, left). When the variable
goes out of scope, it migrates into a slot inside the upvalue itself (Figure4,
right).Becauseaccessisindirectthroughapointerintheupvalue,thismigration
is transparent to any code that reads or writes the variable. Unlike its inner
functions, the function that declares the variable accesses it as it accesses its
| own local | variables: | directly | in the stack. |     |     |
| --------- | ---------- | -------- | ------------- | --- | --- |
Mutable state is shared correctly among closures by creating at most one
3
The techniques used in pure functional languages, such as Haskell, are usually not
| applicable | toprocedural |     | languages. |     |     |
| ---------- | ------------ | --- | ---------- | --- | --- |

upvalue per variable and reusing it as needed. To ensure this uniqueness, Lua
keeps a linked list with all open upvalues (that is, those that still point to the
stack) of a stack (the pending vars list in Figure4). When Lua creates a new
closure, it goes through all its outer local variables. For each one, if it can (cid:12)nd
anopenupvalueinthe list,itreusesthatupvalue.Otherwise,Luacreatesanew
upvalue and links it in the list. Notice that the list search typically probes only
a few nodes, because the list contains at most one entry for each local variable
thatis usedbyanested function.Onceaclosedupvalueis nolongerreferredby
any closure, it is eventually garbagecollected.
It is possible for a function to access an outer local variable that does not
belong to its immediately enclosing function, but to an outer function. In that
case, even by the time the closure is created, the variable may no longer exist
in the stack. Lua solves this case by using (cid:13)at closures [5]. With (cid:13)at closures,
whenever a function accesses an outer variable that is not local to its enclosing
function, the variable also goes to the closure of the enclosing function. Thus,
when a function is instantiated, all variables that go into its closure are either
in the enclosing function’s stack or in the enclosing function’s closure.
6 Threads and Coroutines
Since version 5.0, Lua implements asymmetric coroutines (also called semi-
symmetric coroutines or semi-coroutines) [7]. Those coroutines are supported
by three functions from the Lua standard library: create, resume, and yield.
(These functions live in the coroutine namespace.) The create function re-
ceives a \main" function and creates a new coroutine with that function. It
returns a value of type thread that representsthat coroutine. (Like all values in
Lua,coroutinesare(cid:12)rst-classvalues.)Theresumefunction(re)startsthe execu-
tionofagivencoroutine,callingitsmainfunction.Theyieldfunctionsuspends
the execution of the running coroutine and returns the control to the call that
resumed that coroutine.
Conceptually, each coroutine has its own stack. (Concretely, each coroutine
has two stacks, as we shall discuss in Section7, but we can consider them as
a single abstract stack.) Coroutines in Lua are stackful, in the sense that we
cansuspendacoroutinefrominsideanynumberof nestedcalls.Theinterpreter
simplyputsasidetheentirestackforlateruseandcontinuesrunningonanother
stack. A program can restart any suspended coroutine at will. The garbage
collector collects stacks whose coroutines are no longer accessible.
The combination of stackfulness and (cid:12)rst-class status makes coroutines, as
implementedinLua,isequivalenttoone-shotcontinuations.Assuch,theyallow
programmersto implement severaladvanced controlmechanisms, such ascoop-
erativemultithreading, generators,symmetric coroutines, backtracking,etc.[7].

AkeypointintheimplementationofcoroutinesinLuaisthattheinterpreter
cannot use its internal Cstack to implement calls in the interpreted code. (The
Python community calls an interpreter that follows that restriction a stackless
interpreter[23].) When the main interpreter loop executes a call operation, it
creates a new slot in the stack, adjusts several pointers, and continues the loop
withtheinstructionsofthecalledfunction.Similarly,areturnoperationremoves
the topstackslot,adjustspointers,andcontinuestheloopwith theinstructions
ofthecallingfunction.Notbycoincidence,thatisexactlywhatarealCPUdoes
to perform function calls.
When the interpreter executes a resume, however, it does a recursive call to
the main interpreter function. This new invocation is responsible for executing
the resumed coroutine, using the coroutine stack to perform calls and returns.
When this new loop executes an yield, it returns to the previous interpreter
invocation, leaving the coroutine stack with any pending calls. In other words,
Luausesthe Cstacktokeeptrackof the stackof activecoroutinesatanygiven
time. Each yield returns to the previous interpreter loop, which is the one that
called the corresponding resume.
Asourceofdi(cid:14)cultiesintheimplementationofcoroutinesinsomelanguages
is how to handle references to outer local variables. Because a function running
in a coroutine may have been created in another coroutine, it may refer to
variables in a di(cid:11)erent stack. This leads to what some authors call a cactus
structure[18]. The use of (cid:13)at closures, as we discussed in Section5, avoids this
problem altogether.
7 The Virtual Machine
Lua runs programs by (cid:12)rst compiling them into instructions (\opcodes") for a
virtual machine and then executing those instructions. For each function that
Lua compiles it creates a prototype, which contains an array with the opcodes
forthefunctionandanarrayofLuavalues(TObjects)withallconstants(literal
strings and numerals) used by the function.
For ten years (since 1993, when Lua was (cid:12)rst released), Lua used a stack-
based virtual machine, in various incarnations. Since 2003, with the release of
Lua5.0, Lua uses a register-basedvirtualmachine. This register-basedmachine
also uses a stack, for allocating activation records, wherein the registers live.
When Lua enters a function, it preallocates from the stack an activation record
large enough to hold all the function registers. All local variables are allocated
in registers. As a consequence, access to local variables is specially e(cid:14)cient.
Register-basedcodeavoidsseveral\push"and\pop"instructionsthatstack-
based code needs to move values around the stack. Those instructions are par-
ticularly expensive in Lua, because they involve the copy of a tagged value, as

discussed in Section3. So, the register architecture both avoids excessive copy-
ing of values and reduces the total number of instructions per function. Davis
etal.[6] argue in defense of register-based virtual machines and provide hard
data on the improvement of Java bytecode. Some authors also defend register-
based virtual machines based on their suitability for on-the-(cid:13)y compilation (see
[24], for instance).
Therearetwoproblemsusuallyassociatedwithregister-basedmachines:code
sizeanddecodingoverhead.Aninstructioninaregistermachineneedstospecify
its operands, and so it is typically larger than a corresponding instruction in a
stackmachine.(Forinstance,the size of aninstruction inLua’s virtualmachine
is four bytes, while the size of an instruction in several typical stack machines,
including the ones previously used by Lua, is one or two bytes.) On the other
hand, register machines generate less opcodes than stack machines, so the total
code size is not much larger.
Mostinstructionsinastackmachinehaveimplicitoperands.Thecorrespond-
ing instructions in a register machine must decode their operands from the in-
struction. Such decoding adds overhead to the interpreter. There are several
factors that ameliorate this overhead. First, stack machines also spend some
time manipulating implicit operands (e.g., to increment or decrement the stack
top). Second, because in a register machine all operands are inside the instruc-
tion and the instruction is a machine word, the operand decoding involves only
cheapoperations,suchaslogicaloperations.Moreover,instructionsinstackma-
chines frequently need multi-byte operands. For instance, in the Java VM, goto
and branch instructions use a two-byte displacement. Due to alignment, the in-
terpreter cannot fetch such operands at once (at least not with portable code,
where it must always assume worst-case alignment restrictions). On a register
machine, because the operands are inside the instruction, the interpreter does
not have to fetch them independently.
There are 35instructions in Lua’s virtual machine. Most instructions were
chosen to correspond directly to language constructs: arithmetic, table creation
and indexing, function and method calls, setting variables and getting values.
Thereisalsoasetofconventionaljumpinstructionstoimplementcontrolstruc-
tures. Figure5 shows the complete set, together with a brief summary of what
eachinstructiondoes,usingthefollowingnotation:R(X)meansthe Xthregister.
K(X) means the Xth constant. RK(X) means either R(X) or K(X-k), depending
on the value of X | it is R(X) for values of X smaller thank (a build parame-
ter, typically 250). G[X] means the (cid:12)eldX in the table of globals. U[X]means
the Xth upvalue. Foradetailed discussionof Lua’svirtualmachineinstructions,
see[14, 22].
Registersarekept in the run-time stack, which is essentially an array.Thus,
access to registers is fast. Constants and upvalues are stored in arrays and so

| MOVE      | A B   | R(A)        | := R(B)        |          |         |              |           |
| --------- | ----- | ----------- | -------------- | -------- | ------- | ------------ | --------- |
| LOADK     | A Bx  | R(A)        | := K(Bx)       |          |         |              |           |
| LOADBOOL  | A B C | R(A)        | := (Bool)B;    | if       | (C)     | PC++         |           |
| LOADNIL   | A B   | R(A)        | := ...         | := R(B)  | := nil  |              |           |
| GETUPVAL  | A B   | R(A)        | := U[B]        |          |         |              |           |
| GETGLOBAL | A Bx  | R(A)        | := G[K(Bx)]    |          |         |              |           |
| GETTABLE  | A B C | R(A)        | := R(B)[RK(C)] |          |         |              |           |
| SETGLOBAL | A Bx  | G[K(Bx)]    | :=             | R(A)     |         |              |           |
| SETUPVAL  | A B   | U[B]        | := R(A)        |          |         |              |           |
| SETTABLE  | A B C | R(A)[RK(B)] |                | := RK(C) |         |              |           |
| NEWTABLE  | A B C | R(A)        | := {} (size    | =        | B,C)    |              |           |
| SELF      | A B C | R(A+1)      | := R(B);       | R(A)     | :=      | R(B)[RK(C)]  |           |
| ADD       | A B C | R(A)        | := RK(B)       | + RK(C)  |         |              |           |
| SUB       | A B C | R(A)        | := RK(B)       | - RK(C)  |         |              |           |
| MUL       | A B C | R(A)        | := RK(B)       | * RK(C)  |         |              |           |
| DIV       | A B C | R(A)        | := RK(B)       | / RK(C)  |         |              |           |
| POW       | A B C | R(A)        | := RK(B)       | ^ RK(C)  |         |              |           |
| UNM       | A B   | R(A)        | := -R(B)       |          |         |              |           |
| NOT       | A B   | R(A)        | := not         | R(B)     |         |              |           |
| CONCAT    | A B C | R(A)        | := R(B)        | .. ...   | .. R(C) |              |           |
| JMP       | sBx   | PC +=       | sBx            |          |         |              |           |
| EQ        | A B C | if ((RK(B)  | ==             | RK(C))   | ~=      | A) then PC++ |           |
| LT        | A B C | if ((RK(B)  | <              | RK(C))   | ~=      | A) then PC++ |           |
| LE        | A B C | if ((RK(B)  | <=             | RK(C))   | ~=      | A) then PC++ |           |
| TEST      | A B C | if (R(B)    | <=>            | C) then  | R(A)    | := R(B)      | else PC++ |
CALL A B C R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1))
| TAILCALL | A B C | return        | R(A)(R(A+1), |               | ... | ,R(A+B-1))   |          |
| -------- | ----- | ------------- | ------------ | ------------- | --- | ------------ | -------- |
| RETURN   | A B   | return        | R(A),        | ... ,R(A+B-2) |     | (see         | note)    |
| FORLOOP  | A sBx | R(A)+=R(A+2); |              | if R(A)       | <?= | R(A+1) then  | PC+= sBx |
| TFORLOOP | A C   | R(A+2),       | ...          | ,R(A+2+C)     | :=  | R(A)(R(A+1), | R(A+2)); |
TFORPREP A sBx if type(R(A)) == table then R(A+1):=R(A), R(A):=next;
| SETLIST  | A Bx   | R(A)[Bx-Bx%FPF+i] |                        |           | := R(A+i), | 1 <=            | i <= Bx%FPF+1 |
| -------- | ------ | ----------------- | ---------------------- | --------- | ---------- | --------------- | ------------- |
| SETLISTO | A Bx   |                   |                        |           |            |                 |               |
| CLOSE    | A      | close             | stack                  | variables | up         | to R(A)         |               |
| CLOSURE  | A Bx   | R(A)              | := closure(KPROTO[Bx], |           |            | R(A),           | ... ,R(A+n))  |
|          | Figure | 5:The             | instructions           | in        | Lua’s      | virtual machine |               |
access to them is also fast. The table of globals is an ordinary Lua table. It is
accessedviahashingbutwithgoodperformance,becauseitisindexedonlywith
strings (corresponding to variable names), and strings pre-compute their hash
| values, as | mentioned | in Section2. |     |     |     |     |     |
| ---------- | --------- | ------------ | --- | --- | --- | --- | --- |
The instructions in Lua’s virtual machine take 32bits divided into three or
four(cid:12)elds,asshowninFigure6.TheOP(cid:12)eldidenti(cid:12)estheinstructionandtakes
6bits. The other (cid:12)elds represent operands. Field A is always present and takes
8bits. Fields B and C take 9bits each. They can be combined into an 18-bit
| (cid:12)eld: Bx | (unsigned) and | sBx | (signed). |     |     |     |     |
| --------------- | -------------- | --- | --------- | --- | --- | --- | --- |
Most instructions use a three-address format, where A points to the register
that will hold the result and Band C point tothe operands,whichcan be either

|          | 0 1 2 3   | 4 5 6 7 | 8 9 10111213141516171819202122232425262728293031 |                |            |         |     |     |
| -------- | --------- | ------- | ------------------------------------------------ | -------------- | ---------- | ------- | --- | --- |
|          | OP        |         | A                                                | B              |            | C       |     |     |
|          | OP        |         | A                                                |                | Bx         |         |     |     |
|          | OP        |         | A                                                |                | sBx        |         |     |     |
|          |           |         | Figure                                           | 6: Instruction | layout     |         |     |     |
| function | max (a,b) |         |                                                  |                |            |         |     |     |
| local    | m = a     |         | 1 MOVE                                           | 2 0            | 0 ; R(2)   | = R(0)  |     |     |
| if b     | > a then  |         | 2 LT                                             | 0 0            | 1 ; R(0)   | < R(1)  | ?   |     |
| m =      | b         |         | 3 JMP                                            | 1              | ; to       | 5 (4+1) |     |     |
| end      |           |         | 4 MOVE                                           | 2 1            | 0 ; R(2)   | = R(1)  |     |     |
| return   | m         |         | 5 RETURN                                         | 2 2            | 0 ; return | R(2)    |     |     |
| end      |           |         | 6 RETURN                                         | 0 1            | 0 ; return |         |     |     |
|          |           | Figure  | 7: Bytecode                                      | for a Lua      | function   |         |     |     |
a registeroraconstant (usingthe representationRK(X)explainedabove).With
thisformat,severaltypicaloperationsinLuacanbecodedinasingleinstruction.
For instance, the increment of a local variable, such as a = a + 1, is coded
| ADDx | x y, | x   |     |     |     |     |     | y   |
| ---- | ---- | --- | --- | --- | --- | --- | --- | --- |
as where represents the register holding the local variable and
represents the constant1. An assignment like a = b.f, when both a and b are
local variables, is also coded as the single instruction GETTABLEx y z, where x
is the register for a, y is the register for b, and z is the index of the string
constant"f". (In Lua, the syntax b.f is syntactic sugar for b["f"], that is, b
| indexed | by the string | "f".) |     |     |     |     |     |     |
| ------- | ------------- | ----- | --- | --- | --- | --- | --- | --- |
Branchinstructionsposeadi(cid:14)cultybecausetheyneedtospecifytwooperands
to be compared plus a jump o(cid:11)set. Packingall this data inside a single instruc-
tionwouldlimitjumpo(cid:11)setsto256(assumingasigned9-bit(cid:12)eld).Thesolution
adoptedinLuaisthat,conceptually,atestinstructionsimplyskipsthe nextin-
struction when the test fails; this next instruction is a regularjump, which uses
an18-bito(cid:11)set.Actually,becauseatestinstructionisalwaysfollowedbyajump
instruction, the interpreter executes both instructions together. That is, when
executing a test instruction that succeeds, the interpreter immediately fetches
the next instruction and does the jump, instead of doingit in the next dispatch
cycle. Figure7 shows an example of Lua code and the corresponding bytecode.
| Note the | structure | of the | conditional | and jump | instructions | just | described. |     |
| -------- | --------- | ------ | ----------- | -------- | ------------ | ---- | ---------- | --- |
Figure8 shows a small sample of the optimizations performed by the Lua
compiler.Figure9showsthesamecodecompiledforLua4.0,whichusedastack-

| local a,t,i |        | 1: LOADNIL              | 0 2 0   |           |     |
| ----------- | ------ | ----------------------- | ------- | --------- | --- |
| a=a+i       |        | 2: ADD                  | 0 0 2   |           |     |
| a=a+1       |        | 3: ADD                  | 0 0 250 | ; 1       |     |
| a=t[i]      |        | 4: GETTABLE             | 0 1 2   |           |     |
|             | Figure | 8: Register-basedopcode |         | (Lua 5.0) |     |
| local a,t,i |        | 1: PUSHNIL              | 3       |           |     |
| a=a+i       |        | 2: GETLOCAL             | 0       | ; a       |     |
|             |        | 3: GETLOCAL             | 2       | ; i       |     |
4: ADD
|        |        | 5: SETLOCAL    | 0      | ; a       |     |
| ------ | ------ | -------------- | ------ | --------- | --- |
| a=a+1  |        | 6: GETLOCAL    | 0      | ; a       |     |
|        |        | 7: ADDI        | 1      |           |     |
|        |        | 8: SETLOCAL    | 0      | ; a       |     |
| a=t[i] |        | 9: GETLOCAL    | 1      | ; t       |     |
|        |        | 10: GETINDEXED | 2      | ; i       |     |
|        |        | 11: SETLOCAL   | 0      | ; a       |     |
|        | Figure | 9: Stack-based | opcode | (Lua 4.0) |     |
based virtual machine with 49instructions. Note how the switch to a register-
based virtual machine allowed the generation of much shorter code. Each exe-
cutable statement in this example compiles to a single instruction in Lua5.0,
| but needs three | or four instructions | in  | Lua4.0. |     |     |
| --------------- | -------------------- | --- | ------- | --- | --- |
register window.
| For function | calls, Lua | uses a kind | of  | It evaluates | the call |
| ------------ | ---------- | ----------- | --- | ------------ | -------- |
arguments in successive registers, starting with the (cid:12)rst unused register. When
it performs the call, those registers become part of the activation record of the
called function, which therefore can access its parameters as regular local vari-
ables.Whenthisfunctionreturns,thoseregistersareputbackintotheactivation
| record of the | caller. |     |     |     |     |
| ------------- | ------- | --- | --- | --- | --- |
Lua uses two parallel stacks for function calls. (Actually, each coroutine has
its own pair of stacks, as we discussed in Section6.) One stack has one entry
for each active function. This entry stores the function being called, the return
address when the function does a call, and a base index, which points to the
activation record of the function. The other stack is simply a large array of
Lua values that keeps those activation records.Each activation record keeps all
temporaryvaluesofthefunction(parameters,localvariables,etc.).Actually,we
can see eachentry in the second stackas a variable-sizepart of a corresponding
| entry in the | (cid:12)rst stack. |     |     |     |     |
| ------------ | ------------------ | --- | --- | --- | --- |

8 Conclusion
In this paperwe havepresented the mostinnovativeaspectsof the implementa-
tion of Lua5.0: its register-based virtual machine, the new algorithm for opti-
| mizing tables | used as arrays,and | the implementation |     | of closures. |
| ------------- | ------------------ | ------------------ | --- | ------------ |
To our knowledge, Lua is the (cid:12)rst language in wide use to adopt a register-
basedvirtualmachine.Theoptimizationfortablesallowsatabletobepartially
implemented as an arraywhen it is used that way (that is, when it has enough
keys in a range 1:::n). Its implementation of closures is also unique, combin-
ing the use of an array-based stack with lexically scoped (cid:12)rst-order functions,
| without complex | control-(cid:13)owanalysis. |     |     |     |
| --------------- | --------------------------- | --- | --- | --- |
ThetableinFigure10showssomesimpleperformancecomparisonsbetween
the old implementation and the new one. The tests were run on an Intel Pen-
tium IV machine with 512 Mbytes running Linux 2.6, with Lua compiled with
gcc 3.3. Lua4.0 uses neither the register-based virtual machine (its machine is
stack based) nor the table{arrayoptimization. Lua5’ is Lua5.0 without table{
arrayoptimization,tailcalls,anddynamicstacks(relatedtocoroutines);Lua5’
| is essentially | Lua4.0 with | the new register-basedvirtual |     | machine. |
| -------------- | ----------- | ----------------------------- | --- | -------- |
We took all test cases from The Great Computer Language Shootout [2],
except the (cid:12)rst one (sum), which is a simple loop to add all integers from1
ton.This(cid:12)rsttestspendsmostof itstime inthe virtualmachine;itshowsthat
thenewvirtualmachinecanbemorethantwiceasfastastheoldone.Theother
tests spend more time in other tasks (function calls, table/array access, etc.),
so the gain in the virtual machine has a smaller e(cid:11)ect on the total time. In the
tests that use arrays (sieve, heapsort, and matrix), the combination of the new
virtual machine with the new optimization for arrays can reduce the running
| time up to | 40%. |     |     |     |
| ---------- | ---- | --- | --- | --- |
The complete code of Lua5.0 is available for browsing at Lua’s web site:
http://www.lua.org/source/5.0/.
|     | program         | Lua 4.0         | Lua 5’    | Lua 5.0 |
| --- | --------------- | --------------- | --------- | ------- |
|     | sum (2e7)       | 1.23 0.54       | (44%)0.54 | (44%)   |
|     | (cid:12)bo (30) | 0.95 0.68       | (72%)0.69 | (73%)   |
|     | ack (8)         | 1.00 0.86       | (86%)0.88 | (88%)   |
|     | random          | (1e6) 1.04 0.96 | (92%)0.96 | (92%)   |
|     | sieve (100)     | 0.93 0.82       | (88%)0.57 | (61%)   |
|     | heapsort        | (5e4) 1.08 1.05 | (97%)0.70 | (65%)   |
|     | matrix          | (50) 0.84 0.82  | (98%)0.59 | (70%)   |
Figure 10: Benchmarks (times in seconds; percentages are relative to Lua 4.0)

Acknowledgments
Edgar Toernig provided a key insight into the implementation of closures.
Thatcher Ulrich made a previous implementation of coroutines in Lua4.0 that
workedasaproof-of-conceptforourimplementationinLua5.0.Theauthorsare
partiallysupportedbygrantsfromCNPq(grants302608/2002-8,300392/2003-6,
and 401109/2003-8),FINEP (CT-INFO01/2003),and MicrosoftResearch(2nd
Rotor RFP).
References
1. A.W.Appel. Empiricalandanalyticstudyofstackversusheapcostforlanguages
| with closures. | Journal |                | of Functional | Programming, |           | 6(1):47{74, 1996. |     |
| -------------- | ------- | -------------- | ------------- | ------------ | --------- | ----------------- | --- |
| 2. D.Bagley.   | The     | great computer |               | language     | shootout. |                   |     |
http://www.bagley.org/~doug/shootout/.
3. R.P.Brent. Reducingtheretrievaltimeof scatterstorage techniques. Communi-
| cations       | of the ACM, | 16(2):105{109, |                        | 1973. |     |           |     |
| ------------- | ----------- | -------------- | ---------------------- | ----- | --- | --------- | --- |
| 4. A.Calpini. | The         | great          | Win32 computerlanguage |       |     | shootout. |     |
http://dada.perl.it/shootout/.
5. L.Cardelli. Compiling a functional language. In LISP and Functional Program-
| ming,pages | 208{217, | 1984. |     |     |     |     |     |
| ---------- | -------- | ----- | --- | --- | --- | --- | --- |
6. B.Davis, A.Beatty, K.Casey, D.Gregg, and J.Waldron. The case for virtual
register machines. In Proceedings of the 2003 Workshop on Interpreters, Virtual
| Machines | and Emulators, |     | pages | 41{49. ACM | Press, | 2003. |     |
| -------- | -------------- | --- | ----- | ---------- | ------ | ----- | --- |
7. A.de Moura, N.Rodriguez, and R.Ierusalimschy. Coroutines in Lua. Journal of
| Universal | Computer | Science, | 10(7):910{925, |     | July2004. |     |     |
| --------- | -------- | -------- | -------------- | --- | --------- | --- | --- |
8. M.A.Ertl andD.Gregg. Thestructureandperformance of e(cid:14)cientinterpreters.
| Journal | of Instruction-Level |     | Parallelism, |     | 5:1{25, | Nov.2003. |     |
| ------- | -------------------- | --- | ------------ | --- | ------- | --------- | --- |
9. A.Goldberg and D.Robson. Smalltalk-80: the language and its implementation.
Addison-Wesley,1983.
10. R.E.GriswoldandM.T.Griswold. TheImplementationoftheIconProgramming
| Language. | Princeton | UniversityPress, |     | 1986. |     |     |     |
| --------- | --------- | ---------------- | --- | ----- | --- | --- | --- |
11. R.E. Griswold, J. F. Poage, and I.P. Polonsky. The SNOBOL 4 Programming
| Language. | Prentice-Hall, |     | 1971. |     |     |     |     |
| --------- | -------------- | --- | ----- | --- | --- | --- | --- |
12. R.Ierusalimschy, L.H. deFigueiredo, and W.Celes. The evolution of an exten-
sion language: A history of Lua. In Proceedings of V Brazilian Symposium on
| Programming | Languages, |     | pages | B{14{B{28, | 2001. |     |     |
| ----------- | ---------- | --- | ----- | ---------- | ----- | --- | --- |
13. S.C.Johnson. YACC:Yetanothercompiler compiler. CSTR32, Bell Labs,July
1975.
| 14. K.-H.Man. | Ano-frills |     | introduction | toLua | 5 VM | instructions. |     |
| ------------- | ---------- | --- | ------------ | ----- | ---- | ------------- | --- |
http://luaforge.net/docman/?group_id=83.
15. S.Pemberton and M. Daniels. Pascal Implementation: The P4 Compiler and In-
| terpreter. | Ellis Horwood, |     | 1982. |     |     |     |     |
| ---------- | -------------- | --- | ----- | --- | --- | --- | --- |
16. I.PiumartaandF.Riccardi. Optimizingdirectthreadedcodebyselectiveinlining.
InACM SIGPLAN Conference on Programming Language Design and Implemen-
| tation | (PLDI), pages | 291{300, | Montreal, |     | Canada, | June1998. |     |
| ------ | ------------- | -------- | --------- | --- | ------- | --------- | --- |
17. A.Randal, D.Sugalski, and L.Toetsch. Perl 6 and Parrot Essentials. O’Reilly,
| second          | edition, 2004. |     |          |             |     |                  |       |
| --------------- | -------------- | --- | -------- | ----------- | --- | ---------------- | ----- |
| 18. M.L. Scott. | Programming    |     | Language | Pragmatics. |     | Morgan Kaufmann, | 2000. |
19. M.Serrano. Control (cid:13)ow analysis: a functional language compilation paradigm.
In 10th ACM Symposium on Applied Computing, pages 118{122, Nashville, TN,
Feb.1995.

20. M.Serrano and P.Weis. Bigloo: A portable and optimizing compiler for strict
functionallanguages. In2nd Static AnalysisSymposium,pages366{381, Glasgow,
| Scotland, | Sept.1995. LNCS983. |     |     |     |
| --------- | ------------------- | --- | --- | --- |
21. Z.Shao and A.W. Appel. Space-e(cid:14)cient closure representations. In ACM Con-
| ference on    | Lisp and Functional | Programming, | pages 150{161, | June 1994. |
| ------------- | ------------------- | ------------ | -------------- | ---------- |
| 22. Z.A.Shaw. | The Luavirtual      | machine.     |                |            |
http://www.zedshaw.com/writings/luas-lvm-instructions/luas_lvm_
instructions/luas_lvm_instructions.html.
23. C.Tismer. Continuations and stackless Python. In Proceedings of the 8th Inter-
| national           | Python Conference, | Arlington, | VA, 2000.      |                 |
| ------------------ | ------------------ | ---------- | -------------- | --------------- |
| 24. P.Winterbottom | andR.Pike.         | The design | of the Inferno | virtualmachine. |
http://www.herpolhode.com/rob/hotchips.html.