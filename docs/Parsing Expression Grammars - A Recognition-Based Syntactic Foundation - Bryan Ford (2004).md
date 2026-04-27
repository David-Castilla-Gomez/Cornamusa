|     |     |     | Parsing           |     | Expression |     |           | Grammars: |     |            |     |     |     |     |
| --- | --- | --- | ----------------- | --- | ---------- | --- | --------- | --------- | --- | ---------- | --- | --- | --- | --- |
|     |     | A   | Recognition-Based |     |            |     | Syntactic |           |     | Foundation |     |     |     |     |
BryanFord
MassachusettsInstituteofTechnology
Cambridge,MA
baford@mit.edu
| Abstract |     |     |     |     |     |     | 1   | Introduction |     |     |     |     |     |     |
| -------- | --- | --- | --- | --- | --- | --- | --- | ------------ | --- | --- | --- | --- | --- | --- |
FordecadeswehavebeenusingChomsky’s generative systemof Most language syntax theory and practice is based on generative
grammars, particularly context-free grammars (CFGs) and regu- systems,suchasregularexpressionsandcontext-freegrammars,in
lar expressions (REs), toexpressthe syntaxof programminglan- whichalanguageisdefinedformallybyasetofrulesappliedre-
guages and protocols. The power of generative grammars to ex- cursivelytogeneratestringsofthelanguage. Arecognition-based
press ambiguity is crucial to their original purpose of modelling system, incontrast, definesalanguageintermsofrulesorpredi-
naturallanguages,butthisverypowermakesitunnecessarilydiffi- catesthatdecidewhetherornotagiven stringisinthelanguage.
cultbothtoexpressandtoparsemachine-orientedlanguagesusing Simplelanguagescanbeexpressedeasilyineitherparadigm. For
CFGs. ParsingExpressionGrammars(PEGs)provide an alterna- example,fs2a(cid:3)js=(aa)ngisagenerativedefinitionofatrivial
tive,recognition-basedformalfoundationfordescribingmachine- languageoveraunarycharacterset,whosestringsare“constructed”
orientedsyntax,whichsolvestheambiguityproblembynotintro- byconcatenatingpairsofa’s.Incontrast,fs2a(cid:3)j(jsjmod2=0)g
ducingambiguityinthefirstplace.WhereCFGsexpressnondeter- isarecognition-baseddefinitionofthesamelanguage,inwhicha
ministicchoicebetweenalternatives,PEGsinsteaduseprioritized stringofa’sis“accepted”ifitslengthiseven.
choice. PEGsaddressfrequentlyfeltexpressivenesslimitationsof
CFGs and REs, simplifying syntax definitions and making it un- Whilemostlanguagetheoryadoptsthegenerativeparadigm,most
necessarytoseparatetheirlexicalandhierarchicalcomponents. A practical language applications in computer science involve the
linear-timeparsercanbebuiltforanyPEG,avoidingboththecom- recognition and structural decomposition, or parsing, of strings.
plexityandficklenessofLRparsersandtheinefficiencyofgener- Bridgingthegapfromgenerativedefinitionstopracticalrecogniz-
alizedCFGparsing.WhilePEGsprovidearichsetofoperatorsfor ers is the purpose of our ever-expanding library of parsing algo-
constructinggrammars,theyarereducibletotwominimalrecogni- rithmswithdiversecapabilitiesandtrade-offs[9].
tionschemasdevelopedaround1970,TS/TDPLandgTS/GTDPL,
whicharehereprovenequivalentineffectiverecognitionpower. Chomsky’sgenerativesystemofgrammars,fromwhichtheubiqui-
touscontext-freegrammars(CFGs)andregularexpressions(REs)
|     |     |     |     |     |     |     | arise, | was | originally | designed | as a | formal | tool for modelling | and |
| --- | --- | --- | --- | --- | --- | --- | ------ | --- | ---------- | -------- | ---- | ------ | ------------------ | --- |
CategoriesandSubjectDescriptors
|     |     |     |     |     |     |     | analyzing |     | natural | (human) | languages. | Due | to their | elegance and |
| --- | --- | --- | --- | --- | --- | --- | --------- | --- | ------- | ------- | ---------- | --- | -------- | ------------ |
expressivepower,computerscientistsadoptedgenerativegrammars
| F.4.2 [Mathematical |       | Logic     | and Formal      | Languages]: |        | Gram- |                                               |     |     |     |     |     |     |              |
| ------------------- | ----- | --------- | --------------- | ----------- | ------ | ----- | --------------------------------------------- | --- | --- | --- | --- | --- | --- | ------------ |
|                     |       |           |                 |             |        |       | fordescribingmachine-orientedlanguagesaswell. |     |     |     |     |     |     | Theabilityof |
| mars and            | Other | Rewriting | Systems—Grammar |             | types; | D.3.1 |                                               |     |     |     |     |     |     |              |
aCFGtoexpressambiguoussyntaxisanimportantandpowerful
| [Programming | Languages]: |     | Formal | Definitions | and | Theory— |      |             |            |     |                |     |            |             |
| ------------ | ----------- | --- | ------ | ----------- | --- | ------- | ---- | ----------- | ---------- | --- | -------------- | --- | ---------- | ----------- |
|              |             |     |        |             |     |         | tool | for natural | languages. |     | Unfortunately, |     | this power | gets in the |
Syntax;D.3.4[ProgrammingLanguages]:Processors—Parsing
|     |     |     |     |     |     |     | way      | when | we use        | CFGs | for machine-oriented |     | languages | that are   |
| --- | --- | --- | --- | --- | --- | --- | -------- | ---- | ------------- | ---- | -------------------- | --- | --------- | ---------- |
|     |     |     |     |     |     |     | intended |      | to be precise | and  | unambiguous.         |     | Ambiguity | in CFGs is |
GeneralTerms difficulttoavoidevenwhenwewantto,anditmakesgeneralCFG
parsinganinherentlysuper-linear-timeproblem[14,23].
Languages,Algorithms,Design,Theory
Thispaperdevelopsanalternative,recognition-basedformalfoun-
|     |     |     |     |     |     |     | dation | for | language | syntax, | Parsing | Expression | Grammars | or  |
| --- | --- | --- | --- | --- | --- | --- | ------ | --- | -------- | ------- | ------- | ---------- | -------- | --- |
Keywords
|     |     |     |     |     |     |     | PEGs. | PEGs   | are              | stylistically | similar               | to CFGs | with | RE-like fea- |
| --- | --- | --- | --- | --- | --- | --- | ----- | ------ | ---------------- | ------------- | --------------------- | ------- | ---- | ------------ |
|     |     |     |     |     |     |     | tures | added, | muchlikeExtended |               | Backus-NaurForm(EBNF) |         |      | no-          |
Context-free grammars, regular expressions, parsing expression tation[30,19]. Akeydifferenceisthatinplaceoftheunordered
grammars, BNF, lexical analysis, unified grammars, scannerless choiceoperator‘j’usedtoindicatealternativeexpansionsforanon-
parsing,packratparsing,syntacticpredicates,TDPL,GTDPL terminalinEBNF,PEGsuseaprioritizedchoiceoperator‘=’.This
operatorlistsalternativepatternstobetestedinorder,uncondition-
|     |     |     |     |     |     |     | allyusingthefirstsuccessfulmatch. |     |     |     |     | TheEBNFrules‘A!abj |     |     |
| --- | --- | --- | --- | --- | --- | --- | --------------------------------- | --- | --- | --- | --- | ------------------ | --- | --- |
Permissiontomakedigitalorhardcopiesofallorpartofthisworkforpersonalor a’and‘A!ajab’areequivalentinaCFG,butthePEGrules‘A
classroomuseisgrantedwithoutfeeprovidedthatcopiesarenotmadeordistributed  ab=a’and‘A a=ab’aredifferent. Thesecondalternative
forprofit orcommercialadvantageandthatcopiesbearthisnoticeandthefullcitation inthelatterPEGrulewillneversucceedbecausethefirstchoiceis
onthefirst page.Tocopyotherwise,torepublish,topostonserversortoredistribute
alwaystakeniftheinputstringtoberecognizedbeginswith‘a’.
| tolists,requirespriorspecific |     | permissionand/orafee. |     |     |     |     |     |     |     |     |     |     |     |     |
| ----------------------------- | --- | --------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
POPL’04,January14–16,2004,Venice,Italy.
Copyright2004ACM1-58113-729-X/04/0001...$5.00

|     |     |     |     |     |     |     | # Hierarchical | syntax |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | -------------- | ------ | --- | --- | --- |
APEGmaybeviewedasaformaldescriptionofatop-downparser.
|                |         |           |         |            |             |          | Grammar    | <- Spacing    | Definition+ |     | EndOfFile  |
| -------------- | ------- | --------- | ------- | ---------- | ----------- | -------- | ---------- | ------------- | ----------- | --- | ---------- |
| Two closely    | related | prior     | systems | upon which | this work   | is based |            |               |             |     |            |
|                |         |           |         |            |             |          | Definition | <- Identifier | LEFTARROW   |     | Expression |
| were developed |         | primarily | for the | purpose    | of studying | top-down |            |               |             |     |            |
parsers[4,5].PEGshavefarmoresyntacticexpressivenessthanthe
|                                                    |         |           |                   |     |                 |          | Expression | <- Sequence | (SLASH    | Sequence)* |                 |
| -------------------------------------------------- | ------- | --------- | ----------------- | --- | --------------- | -------- | ---------- | ----------- | --------- | ---------- | --------------- |
| LL(k) language                                     | class   | typically | associated        |     | with top-down   | parsers, |            |             |           |            |                 |
|                                                    |         |           |                   |     |                 |          | Sequence   | <- Prefix*  |           |            |                 |
| however,                                           | and can | express   | all deterministic |     | LR(k) languages | and      |            |             |           |            |                 |
|                                                    |         |           |                   |     |                 |          | Prefix     | <- (AND     | / NOT)?   | Suffix     |                 |
| manyothers,includingsomenon-context-freelanguages. |         |           |                   |     |                 | Despite  |            |             |           |            |                 |
|                                                    |         |           |                   |     |                 |          | Suffix     | <- Primary  | (QUESTION |            | / STAR / PLUS)? |
theirconsiderableexpressivepower,allPEGscanbeparsedinlin-
|          |         |            |           |        |            |         | Primary | <- Identifier | !LEFTARROW |       |     |
| -------- | ------- | ---------- | --------- | ------ | ---------- | ------- | ------- | ------------- | ---------- | ----- | --- |
| ear time | using a | tabular or | memoizing | parser | [8]. These | proper- |         |               |            |       |     |
|          |         |            |           |        |            |         |         | / OPEN        | Expression | CLOSE |     |
tiesstronglysuggestthatCFGsandPEGsdefineincomparablelan-
|                |           |     |             |           |                 |     |     | / Literal | / Class | /   | DOT |
| -------------- | --------- | --- | ----------- | --------- | --------------- | --- | --- | --------- | ------- | --- | --- |
| guage classes, | althougha |     | formalproof | thatthere | arecontext-free |     |     |           |         |     |     |
languagesnotexpressibleviaPEGsappearssurprisinglyelusive.
|                    |              |      |                   |       |                   |          | # Lexical  | syntax        |            |       |         |
| ------------------ | ------------ | ---- | ----------------- | ----- | ----------------- | -------- | ---------- | ------------- | ---------- | ----- | ------- |
|                    |              |      |                   |       |                   |          | Identifier | <- IdentStart | IdentCont* |       | Spacing |
| Besides developing |              | PEGs | asa formalsystem, |       | thispaperpresents |          |            |               |            |       |         |
|                    |              |      |                   |       |                   |          | IdentStart | <- [a-zA-Z_]  |            |       |         |
| pragmatic          | examplesthat |      | demonstrate       | their | suitabilityfor    | describ- |            |               |            |       |         |
|                    |              |      |                   |       |                   |          | IdentCont  | <- IdentStart | /          | [0-9] |         |
ingrealisticmachine-orientedlanguages.Sincetheselanguagesare
generallydesignedtobeunambiguousandlinearlyreadableinthe
|     |     |     |     |     |     |     | Literal | <- [’] | (![’] Char)* | [’] | Spacing |
| --- | --- | --- | --- | --- | --- | --- | ------- | ------ | ------------ | --- | ------- |
firstplace,therecognition-orientednatureofPEGscreatesanatural
|     |     |     |     |     |     |     |     | / ["] | (!["] Char)* | ["] | Spacing |
| --- | --- | --- | --- | --- | --- | --- | --- | ----- | ------------ | --- | ------- |
affinityintermsofsyntacticexpressivenessandparsingefficiency.
|                        |     |     |            |             |     |             | Class | <- ’[’  | (!’]’ Range)* |        | ’]’ Spacing |
| ---------------------- | --- | --- | ---------- | ----------- | --- | ----------- | ----- | ------- | ------------- | ------ | ----------- |
|                        |     |     |            |             |     |             | Range | <- Char | ’-’ Char      | / Char |             |
| Theprimarycontribution |     |     | ofthiswork | istoprovide |     | languageand |       |         |               |        |             |
|                        |     |     |            |             |     |             | Char  | <- ’\\’ | [nrt’"\[\]\\] |        |             |
protocoldesignerswithanewtoolfordescribingsyntaxthatisboth
/ ’\\’ [0-2][0-7][0-7]
practicalandrigorouslyformalized.Asecondarycontributionisto
/ ’\\’ [0-7][0-7]?
renderthisformalismmoreamenabletofurtheranalysisbyprov-
|     |     |     |     |     |     |     |     | / !’\\’ | .   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------- | --- | --- | --- |
ingitsequivalencetotwosimplerformalsystems,originallynamed
| TS (“TMG           | recognition | scheme”)                              |                | and gTS | (“generalized           | TS”) by |           |         |         |     |     |
| ------------------ | ----------- | ------------------------------------- | -------------- | ------- | ----------------------- | ------- | --------- | ------- | ------- | --- | --- |
|                    |             |                                       |                |         |                         |         | LEFTARROW | <- ’<-’ | Spacing |     |     |
| AlexanderBirman    |             | [4,5],                                | in referenceto |         | an earlysyntax-directed |         |           |         |         |     |     |
|                    |             |                                       |                |         |                         |         | SLASH     | <- ’/’  | Spacing |     |     |
| compiler-compiler. |             | ThesesystemswerelatercalledTDPL(“Top- |                |         |                         |         |           |         |         |     |     |
|                    |             |                                       |                |         |                         |         | AND       | <- ’&’  | Spacing |     |     |
DownParsingLanguage”)andGTDPL(“GeneralizedTDPL”)re-
|     |     |     |     |     |     |     | NOT | <- ’!’ | Spacing |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | ------- | --- | --- |
spectivelybyAhoandUllman[3].Byextensionweprovethatwith
|     |     |     |     |     |     |     | QUESTION | <- ’?’ | Spacing |     |     |
| --- | --- | --- | --- | --- | --- | --- | -------- | ------ | ------- | --- | --- |
minorcaveatsTS/TDPLandgTS/GTDPLareequivalentinrecog-
|     |     |     |     |     |     |     | STAR | <- ’*’ | Spacing |     |     |
| --- | --- | --- | --- | --- | --- | --- | ---- | ------ | ------- | --- | --- |
nitionpower,anunexpectedresultcontrarytopriorconjectures[5].
|     |     |     |     |     |     |     | PLUS | <- ’+’ | Spacing |     |     |
| --- | --- | --- | --- | --- | --- | --- | ---- | ------ | ------- | --- | --- |
|     |     |     |     |     |     |     | OPEN | <- ’(’ | Spacing |     |     |
Therestofthispaperisorganizedasfollows.Section2firstdefines
|     |     |     |     |     |     |     | CLOSE | <- ’)’ | Spacing |     |     |
| --- | --- | --- | --- | --- | --- | --- | ----- | ------ | ------- | --- | --- |
PEGsinformallyandpresentsexamplesoftheirusefulnessforde-
|                                             |     |     |     |     |                 |     | DOT | <- ’.’ | Spacing |     |     |
| ------------------------------------------- | --- | --- | --- | --- | --------------- | --- | --- | ------ | ------- | --- | --- |
| scribingpracticalmachine-orientedlanguages. |     |     |     |     | Section3thende- |     |     |        |         |     |     |
finesPEGsformallyandprovessomeoftheirimportantproperties.
|     |     |     |     |     |     |     | Spacing | <- (Space | / Comment)* |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------- | --------- | ----------- | --- | --- |
Section4presentsusefultransformationsonPEGsandprovesthe
|             |           |     |              |         |         |         | Comment | <- ’#’ | (!EndOfLine | .)*       | EndOfLine |
| ----------- | --------- | --- | ------------ | ------- | ------- | ------- | ------- | ------ | ----------- | --------- | --------- |
| main result | regarding | the | reducibility | of PEGs | to TDPL | and GT- |         |        |             |           |           |
|             |           |     |              |         |         |         | Space   | <- ’ ’ | / ’\t’ /    | EndOfLine |           |
DPL.Section5outlinesopenproblemsforfuturestudy,Section6
|     |     |     |     |     |     |     | EndOfLine | <- ’\r\n’ | / ’\n’ | / ’\r’ |     |
| --- | --- | --- | --- | --- | --- | --- | --------- | --------- | ------ | ------ | --- |
describesrelatedwork,andSection7concludes.
|     |     |     |     |     |     |     | EndOfFile | <- !. |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --------- | ----- | --- | --- | --- |
Figure1.PEGformallydescribingitsownASCIIsyntax
| 2 ParsingExpression |     |     | Grammars |     |     |     |     |     |     |     |     |
| ------------------- | --- | --- | -------- | --- | --- | --- | --- | --- | --- | --- | --- |
The?,*,and+operatorsbehaveasincommonregularexpression
Figure1showsanexamplePEG,whichpreciselyspecifiesaprac- syntax,exceptthattheyare“greedy”ratherthannondeterministic.
ticalsyntaxforPEGsusingtheASCIIcharacterset. Theexample The option expression ‘e?’ unconditionally “consumes” the text
PEGdescribesitsowncompletesyntaxincludingalllexicalchar- matched by e if e succeeds, and the repetition expressions ‘e*’
acteristics. Mostelementsofthegrammarshouldbeimmediately and‘e+’alwaysconsumeasmanysuccessivematchesofeaspos-
recognizabletoanyonefamiliarwithCFGsandregularexpressions. sible. The expression ‘a* a’ for example can never match any
Thegrammarconsistsofasetofdefinitionsoftheform‘A<-e’, string. Longest-matchparsingisalmostalwaysthedesiredbehav-
whereAisanonterminalandeisaparsingexpression. Theopera- iorwhereoptionsorrepetitionoccurinpracticalmachine-oriented
torsforconstructingparsingexpressionsaresummarizedinTable1. languages. Manyformsofnon-greedybehaviorarestillavailable
inPEGswhendesired,however,throughtheuseofpredicates.
Singleordoublequotesdelimitstringliterals,andsquarebrackets
indicatecharacterclasses.Literalsandcharacterclassescancontain Theoperators&and!denotesyntacticpredicates[20],whichpro-
C-likeescapecodes,andcharacterclassescanincluderangessuch vide much of the practical expressive power of PEGs. The ex-
as‘a-z’.Theconstant‘.’matchesanysinglecharacter. pression ‘&e’ attempts to match pattern e, then unconditionally
backtrackstothestartingpoint,preservingonlytheknowledgeof
Thesequenceexpression‘e 1 e 2 ’looksforamatchofe 1 immedi- whether e succeeded or failed to match. Conversely, the expres-
atelyfollowedbyamatchofe 2 ,backtrackingtothestartingpoint sion‘!e’failsifesucceeds, butsucceedsifefails. Forexample,
ifeitherpatternfails.Thechoiceexpression‘e 1=e 2 ’firstattempts the subexpression ‘!EndOfLine .’ in the definition for Comment
patterne 1 ,thenattemptse 2 fromthesamestartingpointife 1 fails. in Figure 1, matches any single character as long as the nonter-

| Operator | Type    |     | Precedence |     | Description   |     |     |                            |     |     |     |     |     |     |
| -------- | ------- | --- | ---------- | --- | ------------- | --- | --- | -------------------------- | --- | --- | --- | --- | --- | --- |
|          |         |     |            |     |               |     |     | 2.2 NewSyntaxDesignChoices |     |     |     |     |     |     |
| ’ ’      | primary |     | 5          |     | Literalstring |     |     |                            |     |     |     |     |     |     |
| " "      | primary |     | 5          |     | Literalstring |     |     |                            |     |     |     |     |     |     |
Besidesbeingabletoexpressmanyexistingmachine-orientedlan-
| [ ] | primary     |     | 5   |     | Characterclass |     |     |               |           |          |         |                  |              |            |
| --- | ----------- | --- | --- | --- | -------------- | --- | --- | ------------- | --------- | -------- | ------- | ---------------- | ------------ | ---------- |
|     |             |     |     |     |                |     |     | guages in     | a concise | and      | unified | grammar,         | PEGs also    | create new |
| .   | primary     |     | 5   |     | Anycharacter   |     |     |               |           |          |         |                  |              |            |
|     |             |     |     |     |                |     |     | possibilities | for       | language | syntax  | design.          | Consider for | example a  |
| (e) | primary     |     | 5   |     | Grouping       |     |     |               |           |          |         |                  |              |            |
|     |             |     |     |     |                |     |     | well-known    | problem   | with     | C++     | syntax involving | nested       | template   |
| e?  | unarysuffix |     | 4   |     | Optional       |     |     |               |           |          |         |                  |              |            |
typeexpressions:
| e*  | unarysuffix |     | 4   |     | Zero-or-more  |     |     |                      |     |     |             |     |     |     |
| --- | ----------- | --- | --- | --- | ------------- | --- | --- | -------------------- | --- | --- | ----------- | --- | --- | --- |
| e+  | unarysuffix |     | 4   |     | One-or-more   |     |     |                      |     |     |             |     |     |     |
|     |             |     |     |     |               |     |     | vector<vector<float> |     |     | > MyMatrix; |     |     |     |
| &e  | unaryprefix |     | 3   |     | And-predicate |     |     |                      |     |     |             |     |     |     |
| !e  | unaryprefix |     | 3   |     | Not-predicate |     |     |                      |     |     |             |     |     |     |
Thespacebetweenthetworightanglebracketsisrequiredbecause
| e e | binary |     | 2   |     | Sequence |     |     |     |     |     |     |     |     |     |
| --- | ------ | --- | --- | --- | -------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
1 2 theC++scannerisoblivioustothelanguage’shierarchicalsyntax,
| e 1=e | binary |     | 1   |     | PrioritizedChoice |     |     |     |     |     |     |     |     |     |
| ----- | ------ | --- | --- | --- | ----------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
2 and would otherwiseinterpret the‘>>’ incorrectly asa rightshift
|     |     |     |     |     |     |     |     | operator. | InalanguagedescribedbyaunifiedPEG,however,itis |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ---------------------------------------------- | --- | --- | --- | --- | --- |
Table1.OperatorsforConstructingParsingExpressions
easytodefinethelanguagetopermita‘>>’sequencetobeinter-
pretedaseitheronetokenortwodependingonitscontext:
|     |     |     |     |     |     |     |     | TemplType | <-  | PrimType | (LANGLE | TemplType | RANGLE)? |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | --- | -------- | ------- | --------- | -------- | --- |
minal EndOfLine does not match starting at the same position. ShiftExpr <- PrimExpr (ShiftOper PrimExpr)*
The expression ‘Identifier !LEFTARROW’ in the definition for ShiftOper <- LSHIFT / RSHIFT
Primary,incontrast,matchesanyIdentifierthatisnotfollowed
byaLEFTARROW.Thislatterpredicatepreventstheright-hand-side LANGLE <- ’<’ Spacing
ExpressionatthebeginningofoneDefinitionfromconsuming RANGLE <- ’>’ Spacing
theleft-hand-sideIdentifierofthenextDefinition, eliminat- LSHIFT <- ’<<’ Spacing
ingtheneedforanexplicitdelimiter. Predicatescaninvolvearbi- RSHIFT <- ’>>’ Spacing
traryparsingexpressionsrequiringanyamountof“lookahead.”
Suchpermissivenesscancreateunexpectedsyntacticsubtleties,of
|     |     |     |     |     |     |     |     | course,andcautionandgoodtasteareinorder: |     |     |     |     | apowerfulsyntax |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------------------------------------- | --- | --- | --- | --- | --------------- | --- |
2.1 UnifiedLanguageDefinitions description paradigm also means more rope for the careless lan-
|     |     |     |     |     |     |     |     | guagedesignertohanghimselfwith. |     |     |     | Thetraditionalbehaviorfor |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------------------------- | --- | --- | --- | ------------------------- | --- | --- |
operatortokensisstilleasilyexpressibleifdesired,asfollows:
| Most conventional  |     | syntax descriptions |          | are | split into | two        | parts: a |        |     |            |     |         |     |     |
| ------------------ | --- | ------------------- | -------- | --- | ---------- | ---------- | -------- | ------ | --- | ---------- | --- | ------- | --- | --- |
| CFG to specify     | the | hierarchical        | portion, | and | a set      | of regular | ex-      |        |     |            |     |         |     |     |
|                    |     |                     |          |     |            |            |          | LANGLE |     | <- !LSHIFT | ’<’ | Spacing |     |     |
| pressions defining | the | lexical             | elements | to  | serve as   | terminals  | for      |        |     |            |     |         |     |     |
|                    |     |                     |          |     |            |            |          | RANGLE |     | <- !RSHIFT | ’>’ | Spacing |     |     |
theCFG.CFGsareunsuitableforlexicalsyntaxbecausetheycan-
|     |     |     |     |     |     |     |     | LSHIFT |     | <- ’<<’ | Spacing |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | --- | ------- | ------- | --- | --- | --- |
notdirectlyexpressmanycommonidioms,suchasthegreedyrule
|     |     |     |     |     |     |     |     | RSHIFT |     | <- ’>>’ | Spacing |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ------ | --- | ------- | ------- | --- | --- | --- |
thatusuallyappliestoidentifiersandnumbers,or“negative”syn-
taxsuchastheLiteralruleabove,inwhichquotedstringliterals
Freeinglexicalsyntaxfromtherestrictionsofregularexpressions
| maycontainanycharacterexceptthequotecharacter. |     |     |     |     |     | Regularex- |     |     |     |     |     |     |     |     |
| ---------------------------------------------- | --- | --- | --- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- |
alsoenablestokenstohavehierarchicalcharacteristics,orevento
pressionscannotdescriberecursivesyntax,however,suchaslarge
|     |     |     |     |     |     |     |     | referbacktothehierarchicalportionofthelanguage. |     |     |     |     |     | Pascal-like |
| --- | --- | --- | --- | --- | --- | --- | --- | ----------------------------------------------- | --- | --- | --- | --- | --- | ----------- |
expressionsconstructedinductivelyfromsmallerexpressions.
nestablecomments,forexample,cannotbedescribedbyaregular
expressionbutareeasilyexpressedinaPEG:
| Neither of                | these difficulties |     | exist with                     | PEGs, | as demonstrated |     | by  |         |         |          |     |       |          |     |
| ------------------------- | ------------------ | --- | ------------------------------ | ----- | --------------- | --- | --- | ------- | ------- | -------- | --- | ----- | -------- | --- |
| theunifiedexamplegrammar. |                    |     | Thegreedynatureoftherepetition |       |                 |     |     |         |         |          |     |       |          |     |
|                           |                    |     |                                |       |                 |     |     | Comment | <- ’(*’ | (Comment | /   | !’*)’ | .)* ’*)’ |     |
operatorensuresthatasequenceofletterscanonlybeinterpretedas
asingleIdentifierandnotastwoormoreimmediatelyadjacent,
|     |     |     |     |     |     |     |     | Character | and stringliterals |     | inmostprogramminglanguagesper- |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ------------------ | --- | ------------------------------ | --- | --- | --- |
shorterones.Not-predicatesdescribetheappropriatenegativecon-
mitescapesequencesofsomekind,toexpresseitherspecialchar-
straintsontheelementsthatcanappearinliterals,characterclasses,
|              |               |     |                                |     |     |     |     | actersordynamicstringsubstitutions. |             |         |          | Theseescapesusuallyhave |                    |      |
| ------------ | ------------- | --- | ------------------------------ | --- | --- | --- | --- | ----------------------------------- | ----------- | ------- | -------- | ----------------------- | ------------------ | ---- |
| andcomments. | Thelast‘!’\\’ |     | .’alternativeinthedefinitionof |     |     |     |     |                                     |             |         |          |                         |                    |      |
|              |               |     |                                |     |     |     |     | a highly                            | restrictive | syntax, | however. | A                       | language described | by a |
Charensuresthatthebackslashcannotbeusedinaliteralorchar-
unifiedPEGcouldpermittheuseofarbitraryexpressionsinsuch
acterclassexceptaspartofanescapesequence.
|     |     |     |     |     |     |     |     | escapes, | taking | advantage | of thefull | power | of the language’s | ex- |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ------ | --------- | ---------- | ----- | ----------------- | --- |
pressionsyntax:
| Eachdefinition   | inthe | examplegrammar |     | thatrepresentsadistinct |     |               |     |            |     |     |     |     |     |     |
| ---------------- | ----- | -------------- | --- | ----------------------- | --- | ------------- | --- | ---------- | --- | --- | --- | --- | --- | --- |
| lexical “token,” | such  | as Identifier, |     | Literal,                |     | or LEFTARROW, |     |            |     |     |     |     |     |     |
|                  |       |                |     |                         |     |               |     | Expression | <-  | ... |     |     |     |     |
usestheSpacingnonterminalto“consume”anywhitespaceand/or
|          |             |           |     |            |     |            |     | Primary | <-  | Literal | / ... |     |     |     |
| -------- | ----------- | --------- | --- | ---------- | --- | ---------- | --- | ------- | --- | ------- | ----- | --- | --- | --- |
| comments | immediately | following |     | the token. | The | definition | of  |         |     |         |       |     |     |     |
GrammaralsostartswithSpacinginordertoallowwhitespaceat
|                        |       |                                    |            |            |     |           |     | Literal | <-  | ["] (!["] | Char)*     | ["] |     |     |
| ---------------------- | ----- | ---------------------------------- | ---------- | ---------- | --- | --------- | --- | ------- | --- | --------- | ---------- | --- | --- | --- |
| thebeginningofthefile. |       | Associatingwhitespacewitheachimme- |            |            |     |           |     |         |     |           |            |     |     |     |
|                        |       |                                    |            |            |     |           |     | Char    | <-  | ’\\(’     | Expression | ’)’ |     |     |
| diately preceding      | token | is a                               | convenient | convention |     | for PEGs, | but |         |     |           |            |     |     |     |
|                        |       |                                    |            |            |     |           |     |         | /   | !’\\’     | .          |     |     |     |
whitespacecouldjustaseasilybeassociatedwiththefollowingto-
kenbyreferringtoSpacingatthebeginningofeachtokendefini-
tion.Whitespacecouldevenbetreatedasaseparatekindoftoken, In place of the Java string literal "\u2200" containing the Uni-
consistentwithlexicaltraditions,butdoingsoinaunifiedgrammar code math symbol ‘8’, for example, the literal could be writ-
suchasthisonewouldrequiremanyexplicitreferencestoSpacing ten"\(0x2200)", "\(8704)", oreven "\(Unicode.FOR_ALL)",
throughoutthehierarchicalportionofthesyntax. whereFOR_ALLisaconstantdefinedinaclassnamedUnicode.

2.3 Priorities,NotAmbiguities Like aCFG, a PEGis a purelysyntactic formalism, not by itself
capableofexpressinglanguageswhosesyntaxdependsonsemantic
ThespecificationflexibilityprovidedbyPEGs,andthenewsyntax predicates[20]. AlthoughtheJavalanguagecanbedescribedasa
design choices they create, are not limited to the lexical portions singleunifiedPEG[7],CandC++parsersrequireanincrementally
of a language. Many sensible syntactic constructs are inherently constructedsymboltabletodistinguishbetweenordinaryidentifiers
ambiguouswhenexpressedinaCFG,commonlyleadinglanguage andtypedef-definedtypeidentifiers. Haskellusesaspecialstage
designerstoabandonsyntacticformalityandrelyoninformalmeta- inthe“syntacticpipeline,”insertedbetweenthescannerandparser,
rules to solve these problems. The ubiquitous “dangling ELSE” toimplementthelanguage’slayout-sensitivefeatures.
problem is a classic example, traditionally requiring either an in-
formalmeta-ruleorsevereexpansionandobfuscationoftheCFG.
Thecorrectbehavioriseasilyexpressedwiththeprioritizedchoice 3 FormalDevelopmentofPEGs
operatorinaPEG:
InthissectionwedefinePEGsformallyandexplorekeyproperties.
Statement <- IF Cond THEN Statement ELSE Statement Many of these propertiesand their proofs were inspired by those
/ IF Cond THEN Statement ofthecloselyrelatedTS/TDPLandgTS/GTDPLsystems[4,5,3],
/ ... althoughtheformulationofPEGsissubstantiallydifferent.
The syntax of C++ contains ambiguities that cannot be resolved
with any amount of CFG rewriting, in which certain token se- 3.1 Definition ofaPEG
quencescanbeinterpretedaseitherastatementoradefinition.The
languagespecification[25]resolvesthisproblemwiththeinformal InFigure1weuseda“concrete”ASCII-basedsyntaxforPEGsto
meta-rulethatsuchasequenceisalwaysinterpretedasadefinition illustratethecharacteristicsofPEGsforpracticallanguagedescrip-
if possible. Similarly, thesyntaxof lambdaabstractions, let ex- tionpurposes. Forformalanalysis,however,itismoreconvenient
pressions,andconditionalsinHaskell[11]isunresolvablyambigu- touseanabstractsyntaxforPEGsthatrepresentsonlyitsessential
ousintheCFGparadigm,andishandledintheHaskellspecifica- structure.Webeginthereforebydefiningthisabstractsyntax.
tionwithaninformal“longestmatch”meta-rule.PEGsprovidethe
necessarytools—prioritizedchoice,greedyrepetition,andsyntac- Definition: Aparsingexpressiongrammar(PEG)isa4-tupleG=
ticpredicates—todefinepreciselyhowtoresolvesuchambiguities. (V N;V T;R;e S),whereV
N
isafinitesetofnonterminalsymbols,V
T
is a finite set of terminal symbols, R is a finite set of rules, e is
S
These tools do not make language syntax design easy, of course. a parsing expression termed the start expression, andV N\V T =
Inplaceofhaving todeterminewhethertwopossiblealternatives 0/. Eachruler2Risapair(A;e), whichwewriteA e, where
in a CFG are ambiguous, PEGs present language designers with A2V
N
andeisaparsingexpression. ForanynonterminalA,there
the analogous challenge of determining whether two alternatives is exactly one e such that A e2R. R is therefore a function
ina‘=’expressioncanbereorderedwithoutaffectingthelanguage. fromnonterminalstoexpressions,andwewriteR(A)todenotethe
Thisquestionisoftenobvious,butsometimesisnot,andisundecid- uniqueexpressionesuchthatA e2R.
ableingeneral. AswithdiscoveringambiguityinCFGs,however,
wehavethehopeoffindingautomaticalgorithmstoidentifyorder Wedefineparsingexpressionsinductivelyasfollows. Ife,e ,and
1
sensitivityorinsensitivityconservativelyincommonsituations. e areparsingexpressions,thensois:
2
1. e ,theemptystring
2.4 Quirksand Limitations
2. a,anyterminal,wherea2V
T
.
If the definition of Grammar in Figure 1 did not reference 3. A,anynonterminal,whereA2V N .
EndOfFile at the end, then any ASCII file starting with at least
4. e e ,asequence.
onecorrectDefinitionwouldbeinterpretedasa“correct”gram- 1 2
mar, even if the file has unreadable garbage at the end. This pe- 5. e 1=e 2 ,prioritizedchoice.
culiarity arises from the fact that a parsing expression in a PEG 6. e(cid:3),zero-or-morerepetitions.
can “succeed” without consuming all input text. Weaddress this
minorissuewiththeEndOfFilenonterminal,definedbythepred- 7. !e,anot-predicate.
icateexpression‘!.’,which“matches”theend-of-filebyfailingif
anycharacterisavailableandsucceedingotherwise. Allsubsequentuseoftheunqualifiedterm“grammar”refersspecif-
icallytoparsingexpressiongrammarsasdefinedhere,andtheun-
BothleftandrightrecursionarepermissibleinCFGs,butaswith qualifiedterm“expression”referstoparsingexpressions. Weuse
top-downparsingingeneral,leftrecursionisunavailableinPEGs thevariablesa;b;c;d torepresentterminals,A;B;C;Dfornonter-
becauseitrepresentsadegenerateloop.Forexample,theCFGrules minals,x;y;zforstringsofterminals,andeforparsingexpressions.
‘A ! a A j a’ and ‘A ! A a j a’ represent a series of ‘a’s in a
CFG, but the PEG rule ‘A A a = a’ is degenerate because it ThestructuralrequirementthatRbeafunction,mappingeachnon-
indicates that in order to recognize nonterminal A, a parser must terminalinV toauniqueparsingexpression, precludesthepos-
N
firstrecognizenonterminalA::: Thisrestrictionappliesnotonlyto sibilityofexpressionsinthegrammarcontaining“undefinedrefer-
directleftrecursionasinthisexample,butalsotoindirectormutual ences,”orsubroutinefailures[5].
left recursioninvolvingseveralnonterminals. Sincebothleft and
rightrecursioninaCFGmerelyrepresentrepetition,however,and TheexpressionsetE(G)ofGisthesetcontainingthestartexpres-
repetitioniseasiertoexpressinaPEGusingrepetitionoperators, sione , theexpressionsusedinallgrammarrules,andallsubex-
S
thislimitationisnotaseriousprobleminpractice. pressionsofthoseexpressions.

Arepetition-freegrammarisagrammarwhoseexpressionsetcon- 8. Alternation(case1):If(e 1;xy))(n 1;x),then(e 1=e 2;xy))
tainsonly expressions constructedwithoutusing rule 6above. A (n 1+1;x).Alternativee isfirsttested,andifitsucceeds,the
1
predicate-freegrammarisonewhoseexpressionsetcontainsonly expressione 1=e succeedswithouttestinge .
|     |     |     |     |     |     |     |     |     | 2   |     |     | 2   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
expressionsconstructedwithoutusingrule7.
|     |     |     |     |     |     |     | 9. Alternation |      | (case 2):      | If (e | 1;x) )  | (n 1;f) | and (e 2;x) | )    |
| --- | --- | --- | --- | --- | --- | --- | -------------- | ---- | -------------- | ----- | ------- | ------- | ----------- | ---- |
|     |     |     |     |     |     |     | (n 2;o),       | then | (e 1=e 2;x))(n | 1+n   | 2+1;o). | If      | e 1 fails,  | then |
e 2 istestedanditsresultisusedinstead.
| 3.2 Desugaringthe |          |          | Concrete          | Syntax   |                  |      |                                              |        |             |     |          |             |            |       |
| ----------------- | -------- | -------- | ----------------- | -------- | ---------------- | ---- | -------------------------------------------- | ------ | ----------- | --- | -------- | ----------- | ---------- | ----- |
|                   |          |          |                   |          |                  |      | 10. Zero-or-morerepetitions(repetitioncase): |        |             |     |          |             | If(e;x 1 x | 2 y)) |
|                   |          |          |                   |          |                  |      |                                              |        | (e(cid:3);x |     |          | (e(cid:3);x |            |       |
|                   |          |          |                   |          |                  |      | (n 1;x                                       | 1) and | 2 y))(n     | 2;x | 2), then | 1           | x 2 y))    | (n 1+ |
| The abstract      | syntax   | does not | include character | classes, | the              | “any |                                              |        |             |     |          |             |            |       |
|                   |          |          |                   |          |                  |      | n 2+1;x                                      | 1 x    | 2).         |     |          |             |            |       |
| character”        | constant | ‘.’, the | option operator   | ‘?’,     | the one-or-more- |      |                                              |        |             |     |          |             |            |       |
repetitions operator ‘+’, or the and-predicate operator ‘&’, all of 11. Zero-or-more repetitions (termination case): If (e;x))
|     |     |     |     |     |     |     | 1;f),then(e(cid:3);x))(n |     |     | 1+1;e |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | --- | ----- | --- | --- | --- | --- |
which appear in the concrete syntax. We treat these features of (n ).
theconcretesyntaxas“syntacticsugar,”reducingthemtoabstract 12. Not-predicate (case 1): If (e;xy))(n;x), then (!e;xy))
parsingexpressionsusinglocalsubstitutionsasfollows: (n+1;f). Ifexpressionesucceedsconsuminginputx,then
thesyntacticpredicate!efails.
| (cid:15) Weconsiderthe‘.’                             |              | expressionintheconcretesyntaxtobea |                  |                   |                |     |                                                       |                                              |                                    |     |     |     |     |     |
| ----------------------------------------------------- | ------------ | ---------------------------------- | ---------------- | ----------------- | -------------- | --- | ----------------------------------------------------- | -------------------------------------------- | ---------------------------------- | --- | --- | --- | --- | --- |
|                                                       |              |                                    |                  |                   |                |     | 13. Not-predicate(case2):If(e;x))(n;f),then(!e;x))(n+ |                                              |                                    |     |     |     |     |     |
| characterclasscontainingalloftheterminalsinV          |              |                                    |                  |                   | .              |     |                                                       |                                              |                                    |     |     |     |     |     |
|                                                       |              |                                    |                  |                   | T              |     | 1;e                                                   | ).Ifefails,then!esucceedsbutconsumesnothing. |                                    |     |     |     |     |     |
| (cid:15) If a                                         | 1;a 2;:::;an | are all                            | of the terminals | listed            | in a character |     |                                                       |                                              |                                    |     |     |     |     |     |
| classexpression                                       |              | intheconcretesyntax,               |                  | afterexpandingany |                |     |                                                       |                                              |                                    |     |     |     |     |     |
|                                                       |              |                                    |                  |                   |                |     | Wedefinearelation)+                                   |                                              | frompairs(e;x)tooutcomeso,suchthat |     |     |     |     |     |
| ranges,thenwedesugarthischaracterclassexpressiontothe |              |                                    |                  |                   |                |     |                                                       |                                              | G                                  |     |     |     |     |     |
(e;x))+oiffannexistssuchthat(e;x))(n;o).
| abstractsyntaxexpressiona |     |     | 1=a 2=:::=an. |     |     |     |     |     |     |     |     |     |     |     |
| ------------------------- | --- | --- | ------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
(cid:15) Wedesugaranoptionexpressione?intheconcretesyntaxto If(e;x))+yfory2V(cid:3),wesaythatematchesxinG.If(e;x))+
| e d=e | ,wheree | isthedesugaringofe. |     |     |     |     |                     |     | T                 |     |     |                  |     |     |
| ----- | ------- | ------------------- | --- | --- | --- | --- | ------------------- | --- | ----------------- | --- | --- | ---------------- | --- | --- |
|       |         | d                   |     |     |     |     | f,wesaythatefailson |     | xinG.ThematchsetM |     |     | G(e)ofexpression |     |     |
(cid:15) Wedesugaraone-or-more-repetitionsexpressione+toe e(cid:3) , einGisthesetofinputsxsuchthatematchesxinG.
d d
| wheree | isthedesugaringofe. |     |     |     |     |     |     |     |     |     |     |     |     |     |
| ------ | ------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
d Anexpressionehandlesastringx2V(cid:3)ifiteithermatchesorfails
T
| (cid:15) We | desugar | an and-predicate | &e to | !(!e d), | where e | is the |         |                       |     |     |                         |     |     |     |
| ----------- | ------- | ---------------- | ----- | -------- | ------- | ------ | ------- | --------------------- | --- | --- | ----------------------- | --- | --- | --- |
|             |         |                  |       |          |         | d      | onxinG. | AgrammarGhandlesstrin |     |     | gxifitsstartexpressione |     |     | S   |
desugaringofe. handlesx.Giscompleteifithandlesallstringsx2V(cid:3).
T
|                    |     |     |             |     |     |     | Two expressions |           | e 1 and e 2 | are equivalent, |          | written | e 1 (cid:16)  | e 2 , if |
| ------------------ | --- | --- | ----------- | --- | --- | --- | --------------- | --------- | ----------- | --------------- | -------- | ------- | ------------- | -------- |
| 3.3 Interpretation |     |     | ofa Grammar |     |     |     | 1;x))+          |           | 2;x))+      |                 |          |         |               |          |
|                    |     |     |             |     |     |     | (e              | o implies | (e          | o               | and vice | versa.  | The resulting |          |
stepcountsneednotbethesame.
| Definition: | To  | formalize | the syntactic | meaning | of a grammar |     |     |     |     |     |     |     |     |     |
| ----------- | --- | --------- | ------------- | ------- | ------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- |
G=(V N;V T;R;e S),wedefinearelation)G frompairsoftheform Theorem:If(e;x))(n;y),thenyisaprefixofx:9z(x=yz).
| (e;x) to pairs | of  | the form (n;o), | where e | is a parsing | expression, |     |     |     |     |     |     |     |     |     |
| -------------- | --- | --------------- | ------- | ------------ | ----------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
x2V(cid:3) isaninputstringtoberecognized,n(cid:21)0servesasa“step Proof: By induction on an integer variable m(cid:21)0, using as the
T
counter,”ando2V(cid:3)[ffgindicatestheresultofarecognitionat- inductionhypothesisthepropositionthatthedesiredpropertyholds
T
tempt.The“output”oofasuccessfulmatchistheportionofthein- foralle;x;n(cid:20)m,andy.
putstringrecognizedand“consumed,”whileadistinguishedsym-
bol f 62V T indicatesfailure. For((e;x);(n;o))2)G wewillwrite Theorem: If(e;x))(n 1;o 1)and(e;x))(n 2;o 2), thenn 1=n 2
(e;x))(n;o), with thereferenceto Gbeingimplied. Wedefine ando 1=o 2 .Thatis,therelation)G isafunction.
)G inductivelyasfollows:
|     |     |     |     |     |     |     | Proof: Byinductiononavariablem(cid:21)0,usingtheinductionhy- |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------------------------------------------------------------ | --- | --- | --- | --- | --- | --- | --- |
1. Empty:(e ;x))(1;e )foranyx2V(cid:3). pothesisthatthepropositionholds foralle;x;n 1(cid:20)m;n 2(cid:20)m;o 1 ,
|     |     |     |     | T   |     |     | ando . | Thisinductiontechniquewillsubsequentlybereferredto |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | ------ | -------------------------------------------------- | --- | --- | --- | --- | --- | --- |
2
2. Terminal(successcase):(a;ax))(1;a)ifa2V ,x2V (cid:3). simplyasinductiononstepcountsof)G .
|             |     |                 |        |         | T       | T      |          |              |            |     |               |            |     |       |
| ----------- | --- | --------------- | ------ | ------- | ------- | ------ | -------- | ------------ | ---------- | --- | ------------- | ---------- | --- | ----- |
| 3. Terminal |     | (failure case): | (a;bx) | ) (1;f) | if a 6= | b, and |          |              |            |     |               |            |     |       |
|             |     |                 |        |         |         |        | Theorem: | A repetition | expression |     | e(cid:3) does | not handle | any | input |
(a;e ))(1;f).
stringxonwhichesucceedswithoutconsuminginput:foranyx2
4. Nonterminal: (A;x))(n+1;o)ifA e2Rand(e;x)) V (cid:3),if(e;x))(n 1;e ),then(e(cid:3);x)6)(n 2;o 2)foranyn 2;o .Wecall
|             |     |                 |           |          |          |        | T                             |     |     |     |     |     | 2   |     |
| ----------- | --- | --------------- | --------- | -------- | -------- | ------ | ----------------------------- | --- | --- | --- | --- | --- | --- | --- |
| (n;o).      |     |                 |           |          |          |        | thisthe(cid:3)-loopcondition. |     |     |     |     |     |     |     |
| 5. Sequence |     | (success case): | If (e 1;x | 1 x 2 y) | ) (n 1;x | 1) and |                               |     |     |     |     |     |     |     |
Proof:Byinductiononstepcounts.
| (e 2;x                                       | 2 y))(n | 2;x 2),then(e             | 1 e 2;x 1 x 2           | y))(n   | 1+n 2+1;x       | 1 x 2). |                                         |     |     |     |     |                     |     |     |
| -------------------------------------------- | ------- | ------------------------- | ----------------------- | ------- | --------------- | ------- | --------------------------------------- | --- | --- | --- | --- | ------------------- | --- | --- |
| Expressionse                                 |         | 1 ande 2                  | arematchedinsequence,   |         | and             | ifeach  |                                         |     |     |     |     |                     |     |     |
| succeedsandconsumesinputportionsx            |         |                           |                         | 1 andx  | 2 respectively, |         |                                         |     |     |     |     |                     |     |     |
| thenthesequencesucceedsandconsumesthestringx |         |                           |                         |         |                 | 1 x 2 . |                                         |     |     |     |     |                     |     |     |
|                                              |         |                           |                         |         |                 |         | 3.4 LanguageProperties                  |     |     |     |     |                     |     |     |
| 6. Sequence                                  |         | (failure case             | 1): If                  | (e 1;x) | ) (n 1;f),      | then    |                                         |     |     |     |     |                     |     |     |
| (e 1 e                                       | 2;x))(n | 1+1;f).                   | Ife 1 istestedandfails, |         | thenthese-      |         |                                         |     |     |     |     |                     |     |     |
|                                              |         |                           |                         |         |                 |         | Thissectiondescribespropertiesofparsing |     |     |     |     | expressionlanguages |     |     |
| quencee                                      | 1 e     | 2 failswithoutattemptinge |                         | 2 ,     |                 |         |                                         |     |     |     |     |                     |     |     |
(PELs),theclassoflanguagesthatcanbeexpressedbyPEGs.PELs
7. Sequence (failure case 2): If (e 1;x 1 y) ) (n 1;x 1) and areclosedunderunion,intersection, andcomplement. Itisunde-
(e 2;y))(n 2;f), then (e 1 e 2;x 1 y))(n 1+n 2+1;f). If e 1 cidableingeneralwhetheraPEGrepresentsanonemptylanguage,
succeedsbute 2 fails,thenthesequenceexpressionfails. orwhethertwoPEGsrepresentthesamelanguage.

Definition: ThelanguageL(G)ofaPEGG=(V N;V T;R;e S)isthe (cid:15) D &:&(A!:)B!:
| setofstringsx2V | (cid:3)forwhichthestartexpressione |     |     |     | matchesx. |                                      |     |     |     |                      |                |
| --------------- | ---------------------------------- | --- | --- | --- | --------- | ------------------------------------ | --- | --- | --- | -------------------- | -------------- |
|                 | T                                  |     |     | S   |           |                                      |     |     |     |                      |                |
|                 |                                    |     |     |     |           | NonterminalAmatchesstringsoftheformx |     |     |     | i1 x i2 :::x im a im | :::a i2 a i1 , |
Note that the start expression e only needs to succeed on input whileB matchesstrings ofthe formy i1 y i2 :::y im a im :::a i2 a i1 . The
S
stringxfor xtobeincluded inL(G); e neednot consumeall of nonterminalDusestheand-predicateoperatortomatchonlystrings
S
stringx.Forexample,thetrivialgrammar(fg;V T;fg;e )recognizes matching both A and B, representing solutions to the correspon-
the language V(cid:3) and not just the empty string, because the start dence problem. The &: at the beginning of the definition of D
T
expressione always succeedseventhoughitdoesnotexamineor (desugaredappropriately)ensuresthatemptysolutionsarenotal-
consumeanyinput. ThisdefinitioncontrastswithTSandgTS,in lowed, and the !: after the references to A and B ensure that the
whichpartiallyconsumedinputstringsareexcludedfromthelan- completeinputisconsumedineachcase. Analgorithmtodecide
guageandclassifiedaspartial-acceptancefailures[5]. whetherL(G)isnonemptycouldthereforebeusedtosolvethecor-
respondenceproblemC,yieldingthedesiredresult.
| Definition: | AlanguageLoveranalphabetV |     |     | T isaparsingexpres- |     |     |     |     |     |     |     |
| ----------- | ------------------------- | --- | --- | ------------------- | --- | --- | --- | --- | --- | --- | --- |
sionlanguage(PEL)iffthereexistsaparsingexpressiongrammar Definition: TwoPEGsG 1 andG 2 areequivalentiftheyrecognize
| GwhoselanguageisL. |     |     |     |     |     | thesamelanguage:L(G |     | 1)=L(G | 2). |     |     |
| ------------------ | --- | --- | --- | --- | --- | ------------------- | --- | ------ | --- | --- | --- |
Theorem: Theclassofparsingexpressionlanguagesisclosedun- Theorem:TheequivalenceoftwoarbitraryPEGsisundecidable.
derunion,intersection,andcomplement.
Proof: AnalgorithmtodecidetheequivalenceoftwoPEGscould
|     |     |     |     | 1;V T;R1;e1 |     |     |     |     |     |     |     |
| --- | --- | --- | --- | ----------- | --- | --- | --- | --- | --- | --- | --- |
Proof: SupposewehavetwogrammarsG 1=(V )and alsobeusedtodecidethenon-emptinessproblemabove,simplyby
|     |     |     |     | N   | S   |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
G 2=(V 2;V T;R2;e2 )respectively,describinglanguagesL(G 1)and comparingthegrammartobetestedagainstatrivialgrammarfor
| N                                | S              |           |                        |         |       |                        |     |     |     |     |     |
| -------------------------------- | -------------- | --------- | ---------------------- | ------- | ----- | ---------------------- | --- | --- | --- | --- | --- |
|                                  |                |           |                        | 1\V     | 2=0/, | theemptylanguage.      |     |     |     |     |     |
| L(G 2). Assume                   | without        | loss of   | generality,            | thatV N | N by  |                        |     |     |     |     |     |
| renamingnonterminalsifnecessary. |                |           | Wecanformanewgrammar   |         |       |                        |     |     |     |     |     |
| G0=(V 1[V                        | 2;V T;R1[R2;e0 | ),wheree0 |                        |         |       | 3.5 AnalysisofGrammars |     |     |     |     |     |
| N                                | N              | S         | S isoneofthefollowing: |         |       |                        |     |     |     |     |     |
Weoftenwouldliketoanalyzethebehaviorofaparticulargram-
| (cid:15) Ife0 | =e1 =e2 ,thenL(G0)=L(G |     | 1)[L(G | 2). |     |     |     |     |     |     |     |
| ------------- | ---------------------- | --- | ------ | --- | --- | --- | --- | --- | --- | --- | --- |
S S S maroverarbitraryinputstrings. Whilemanyinterestingproperties
Ife0 =&e1 e2 ,thenL(G0)=L(G of PEGs are undecidable in general, conservative analysis proves
| (cid:15)      |                   |                   | 1)\L(G | 2). |     |                                            |                                                |     |     |     |     |
| ------------- | ----------------- | ----------------- | ------ | --- | --- | ------------------------------------------ | ---------------------------------------------- | --- | --- | --- | --- |
|               | S S S             |                   |        |     |     | usefulandadequateformanypracticalpurposes. |                                                |     |     |     |     |
| (cid:15) Ife0 | =!e1 ,thenL(G0)=V | (cid:3)(cid:0)L(G | 1).    |     |     |                                            |                                                |     |     |     |     |
|               | S S               | T                 |        |     |     |                                            |                                                |     |     |     |     |
|               |                   |                   |        |     |     | Theorem:                                   | Itisundecidablewhetheranarbitrarygrammariscom- |     |     |     |     |
Theorem:TheclassofPELsincludesnon-context-freelanguages. plete:thatis,whetheriteithersucceedsorfailsonallinputstrings.
Proof: The classic example language anbncn is not context-free, Proof:SupposewehaveanarbitrarygrammarG=(V N;V T;R;e S),
butwecanrecognizeitwithaPEGG=(fA;B;Dg;fa;b;cg;R;D), and we define a new grammar G0 =(V 0;V T;R0;e0 ), whereV 0 =
|     |     |     |     |     |     |     |     |     |     | N S | N   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
whereRcontainsthefollowingdefinitions: V N[fAg,A62V ,R0=R[fA &e Ag,ande0 =A. IfG’sstart
|     |     |     |     |     |     |            | N          |     | S                | S            |            |
| --- | --- | --- | --- | --- | --- | ---------- | ---------- | --- | ---------------- | ------------ | ---------- |
|     |     |     |     |     |     | expression | e succeeds | on  | any input string | x, then this | input will |
S
A   aAb=e causeadegenerateloopinG0 vianonterminalA, soG0 isincom-
B   bBc=e plete.IfL(G)isempty,however,thenG0iscompleteandalsofails
D   &(A!b)a(cid:3)B!: onallinputs.AnalgorithmtodecidewhetherG0iscompletewould
thereforeallowustodecidewhetherGisempty,whichhasalready
Theorem: ItisundecidableingeneralwhetherthelanguageL(G) beenshownundecidable.
ofanarbitraryparsingexpressiongrammarGisempty.
|     |     |     |     |     |     | Definition: | Wedefinearelation*G |     | consistingofpairsoftheform |     |     |
| --- | --- | --- | --- | --- | --- | ----------- | ------------------- | --- | -------------------------- | --- | --- |
Proof: We first prove in the same way as for CFGs [3] that it is (e;o), where e is an expression and o2f0;1;fg. We will write
undecidablewhethertheintersectionofthelanguagesoftwoPEGs e*ofor(e;o)2*G ,withthereferencetoGbeingimplied. This
is empty. SincePELsareclosedunderintersection, analgorithm relation represents an abstract simulation of the )G relation. If
totesttheemptinessofthelanguageL(G)ofanyGcouldbeused e*0,thenemightsucceedonsomeinputstringwhileconsuming
totestwhetherL(G 1)\L(G 2)isempty,implyingthatemptinessis noinput. Ife*1,thenemightsucceedwhileconsumingatleast
undecidableaswell. oneterminal. Ife* f, thenemightfailonsomeinput. Wewill
|     |     |     |     |     |     | usethevariablestorepresenta*G |     |     | outcomeofeither0or1. |     | We  |
| --- | --- | --- | --- | --- | --- | ----------------------------- | --- | --- | -------------------- | --- | --- |
Given an instance C = (x 1;y 1);:::;(xn;yn) of Post’s correspon- definethesimulationrelation*G inductivelyasfollows:
| denceproblem                 | overan               | alphabetS | , itis known | tobe         | undecidable       |          |     |     |     |     |     |
| ---------------------------- | -------------------- | --------- | ------------ | ------------ | ----------------- | -------- | --- | --- | --- | --- | --- |
| whether                      | there is a non-empty | string    | w that       | can be built | from el-          | 1. e *0. |     |     |     |     |     |
| ements ofC                   | such that            | w=x x     | :::x =y      | y :::y       | , where 1(cid:20) |          |     |     |     |     |     |
|                              |                      | i1 i2     | im           | i1 i2 im     |                   | 2. a*1.  |     |     |     |     |     |
| i j(cid:20)nforeach1(cid:20) | j(cid:20)m.          |           |              |              |                   |          |     |     |     |     |     |
3. a* f.
| W e bu il da | g r a m m a r G  | = ( V ;V ;R | ; D )w h   | e r e V = f      | A ; B ; D g, an d |           |         |     |     |     |     |
| ------------ | ---------------- | ----------- | ---------- | ---------------- | ----------------- | --------- | ------- | --- | --- | --- | --- |
|              |                  | N T         |            | N                |                   | 4. A*oifR | G(A)*o. |     |     |     |     |
| V = S [      | fa ; : :: ;a g . | Th e a in V | a re di st | i n c t ter m in | a l s n ot in S , |           |         |     |     |     |     |
| T            | 1 n              | i           | T          |                  |                   |           |         |     |     |     |     |
whichwillserveasmarkersassociatedwiththeelementsofC. R 5. e 1 e 2*0ife 1*0ande 2*0.
| containsthefollowingthreerules: |     |     |     |     |     | e 1 e 2*1ife |     | 1*1ande | 2*s. |     |     |
| ------------------------------- | --- | --- | --- | --- | --- | ------------ | --- | ------- | ---- | --- | --- |
|                                 |     |     |     |     |     | e 1 e 2*1ife |     | 1*sande | 2*1. |     |     |
2=:::=xnAan=e
| (cid:15) A x | 1 Aa 1=x 2 Aa |     |     |     |     | 6. e 1 e 2* | f ife | 1* f. |     |     |     |
| ------------ | ------------- | --- | --- | --- | --- | ----------- | ----- | ----- | --- | --- | --- |
2=:::=ynBan=e
| (cid:15) B y | 1 Ba 1=y 2 Ba |     |     |     |     | 7. e 1 e 2* | f ife | 1*sande | 2* f. |     |     |
| ------------ | ------------- | --- | --- | --- | --- | ----------- | ----- | ------- | ----- | --- | --- |

8. e 1=e 2*sife 1*s. (cid:15) ForanonterminalA,theinductionhypothesisallowsustoas-
|                  |     |        |      |     |     | s u m  | e t ha t R | ( A ) h a n d  | l e s al l st  | rin g soflengthn+1;therefore |     |     |
| ---------------- | --- | ------ | ---- | --- | --- | ------ | ---------- | -------------- | -------------- | ---------------------------- | --- | --- |
| 9. e 1=e 2*oife  | 1*  | f ande | 2*o. |     |     |        | G          |                |                |                              |     |     |
|                  |     |        |      |     |     | s o do | e s A b y  | t h e d e fi n | i t io n o f ) | .                            |     |     |
| e(cid:3)*1ife*1, |     |        |      |     |     |        |            |                |                | G                            |     |     |
10.
|                |        |     |     |     |     | (cid:15) Forasequencee |                                                 | 1 e 2 ,wecanassumethate |           |                          | 1 handlesallstrings |     |
| -------------- | ------ | --- | --- | --- | --- | ---------------------- | ----------------------------------------------- | ----------------------- | --------- | ------------------------ | ------------------- | --- |
| e(cid:3)*0ife* |        |     |     |     |     |                        |                                                 |                         | 1;x))(n;e |                          |                     |     |
| 11.            | f.     |     |     |     |     | xoflengthn+1.          |                                                 | If(e                    |           | ),thene                  | 1*0,soWF(e          | 2)  |
|                |        |     |     |     |     | appliesande            |                                                 | 2 alsohandlesx.         |           | If(e 1;x))(n;y)forjyj>0, |                     |     |
| 12. !e* f      | ife*s. |     |     |     |     |                        |                                                 |                         |           |                          |                     |     |
|                |        |     |     |     |     | thene                  | 2 onlyneedstohandlestringsoflengthnorless,which |                         |           |                          |                     |     |
| 13. !e*0ife*   | f.     |     |     |     |     |                        |                                                 |                         |           |                          |                     |     |
|                |        |     |     |     |     | isgiven.If(e           |                                                 | 1;x))(n;f),thene        |           | 2 isnotused.             |                     |     |
Fore(cid:3),theWF(e)conditionensuresthate
|     |     |     |     |     |     | (cid:15) |     |     |     |     | 1 handlesinputsof |     |
| --- | --- | --- | --- | --- | --- | -------- | --- | --- | --- | --- | ----------------- | --- |
Becausethisrelationdoesnotdependontheinputstring,andthere
lengthn+1,andthee6*0conditionensuresthattherecursive
are afinite number ofrelevant expressionsin agrammar, wecan e(cid:3)
|     |     |     |     |     |     | dependency |     | on in | the success | case only | needs | to handle |
| --- | --- | --- | --- | --- | --- | ---------- | --- | ----- | ----------- | --------- | ----- | --------- |
computethisrelationoveranygrammarbyapplyingtheaboverules
stringsoflengthnorless.
iterativelyuntilwereachafixedpoint.
Theorem:Awell-formedgrammarGiscomplete.
| Theorem:Therelation*G |     | summarizes)G |     | asfollows: |     |     |     |     |     |     |     |     |
| --------------------- | --- | ------------ | --- | ---------- | --- | --- | --- | --- | --- | --- | --- | --- |
Proof: Byinductionoverthelengthofinputstrings,eachexpres-
If(e;x))G(n;e
(cid:15) ),thene*0. sioninE(G)handleseveryinputstring. SinceG’sstartexpression
If(e;x))G(n;y)andjyj>0,thene*1. e isinE(G),theconclusionfollows.
| (cid:15)                       |     |     |     |     |     | S   |     |     |     |     |     |     |
| ------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| (cid:15) If(e;x))G(n;f),thene* |     |     | f.  |     |     |     |     |     |     |     |     |     |
3.7 GrammarIdentities
| Proof: Byinductionoverthestepcountsoftherelation)G |     |     |     |     | . The |     |     |     |     |     |     |     |
| -------------------------------------------------- | --- | --- | --- | --- | ----- | --- | --- | --- | --- | --- | --- | --- |
definition rules for *G above correspond one-to-one to the rules A number of important identities allow PEGs to be transformed
for)G .Theconclusionineachcasefollowsimmediatelyfromthe without changing the language they represent. Wewill use these
inductivehypothesis,exceptinthecasesfortherepetitionoperator, identitiesinsubsequentresults.
whichrequirethe(cid:3)-loopconditiontheoremfromSection3.3.
|     |     |     |     |     |     | Theorem:                | The        | sequence     | and alternation | operators |                  | are asso-   |
| --- | --- | --- | --- | --- | --- | ----------------------- | ---------- | ------------ | --------------- | --------- | ---------------- | ----------- |
|     |     |     |     |     |     | ciative under           | expression | equivalence: |                 | e 1(e 2 e | 3) (cid:16) (e 1 | e 2)e 3 and |
|     |     |     |     |     |     | e 1=(e 2=e 3)(cid:16)(e | 1=e        | 2)=e 3 .     |                 |           |                  |             |
3.6 Well-FormedGrammars
|     |     |     |     |     |     | Proof:Trivial,fromthedefinitionof)G |     |     |     | .   |     |     |
| --- | --- | --- | --- | --- | --- | ----------------------------------- | --- | --- | --- | --- | --- | --- |
Awell-formedgrammarisagrammarthatcontainsnodirectlyor
mutuallyleft-recursiverules,suchas‘A Aa=a’,whichcould
Theorem:Sequenceoperatorscanbedistributedintochoiceoper-
| preventthegrammarfromhandlinganyinputstring. |     |     |     |     | Thischeck- |              |          |        |            |                        |     |               |
| -------------------------------------------- | --- | --- | --- | --- | ---------- | ------------ | -------- | ------ | ---------- | ---------------------- | --- | ------------- |
|                                              |     |     |     |     |            | ators on the | left but | not on | the right: | e 1(e 2=e 3)(cid:16)(e | 1 e | 2)=(e 1 e 3), |
ablestructuralpropertyimpliescompleteness,whilebeingpermis-
|                            |            |               |                               |       |                 | but(e 1=e                                 | 2)e 36(cid:16)(e | 1 e 3)=(e 2 | e 3). |     |             |           |
| -------------------------- | ---------- | ------------- | ----------------------------- | ----- | --------------- | ----------------------------------------- | ---------------- | ----------- | ----- | --- | ----------- | --------- |
| siveenoughformostpurposes. |            |               | Agrammarcanhaveleft-recursive |       |                 |                                           |                  |             |       |     |             |           |
| rulesbut stillbe           | completeif | itsdegenerate |                               | loops | are actuallyun- |                                           |                  |             |       |     |             |           |
|                            |            |               |                               |       |                 | Proof: Intheleft-sidecase,theexpression(e |                  |             |       | 1   | e 2)=(e 1 e | 3)invokes |
reachable,butwehavelittleneedforsuchgrammarsinpractice.
e 1 twicefromthesamestartingpoint—onthesameinputstring—
|             |                          |     |     |            |         | making its | result | the same | as the | factored e 1(e | 2=e 3) expression. |     |
| ----------- | ------------------------ | --- | --- | ---------- | ------- | ---------- | ------ | -------- | ------ | -------------- | ------------------ | --- |
| Definition: | WedefineaninductivesetWF |     |     | asfollows. | Wewrite |            |        |          |        |                |                    |     |
G In the right-side case, however, suppose that e 1 succeeds but e 3
| WF(e)fore2WF | ,withthereferencetoGbeingimplied,tomean |     |     |     |     |     |     |     |     |     |     |     |
| ------------ | --------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
G fails.Intheexpression(e 1=e 2)e 3 ,thefailureofe 3 causesthewhole
thatexpressioneiswell-formedinG.
|     |     |     |     |     |     | expressiontofail. |     | In(e 1 e 3)=(e    | 2 e 3),however, |              | thefirstinstanceof |             |
| --- | --- | --- | --- | --- | --- | ----------------- | --- | ----------------- | --------------- | ------------ | ------------------ | ----------- |
|     |     |     |     |     |     | e 3 only causes   | the | first alternative |                 | to fail; the | second             | alternative |
WF(e
1. ). willthenbetried,inwhichthee 3 mightsucceedife 2 consumesa
|     |     |     |     |     |     | differentamountofinputthane |     |     | 1 did. |     |     |     |
| --- | --- | --- | --- | --- | --- | --------------------------- | --- | --- | ------ | --- | --- | --- |
2. WF(a).
3. WF(A)ifWF(R G(A)). Theorem:Predicatescanbemovedleftwithinsequencesdistribu-
4. WF(e e 2)ifWF(e 1)ande 1*0impliesWF(e 2). tivelyasfollows:e 1 !e 2(cid:16)!(e 1 e 2)e 1 .
1
| 5. WF(e 1=e | 2)ifWF(e | 1)andWF(e | 2). |     |     |           |                  |     |                                    |     |     |     |
| ----------- | -------- | --------- | --- | --- | --- | --------- | ---------------- | --- | ---------------------------------- | --- | --- | --- |
|             |          |           |     |     |     | Proof:Ife | 1 succeeds,thene |     | 2 istestedstartingatthesamepointin |     |     |     |
6. WF(e(cid:3))ifWF(e)ande6*0. eachcase,resultinginthesameoverallbehavior; thesecondcase
|     |     |     |     |     |     | merelyinvokese | 1   | twiceatthesameposition. |     |     | Ife 1 fails, | thenthe |
| --- | --- | --- | --- | --- | --- | -------------- | --- | ----------------------- | --- | --- | ------------ | ------- |
7. WF(!e)ifWF(e).
|     |     |     |     |     |     | predicateine                         | 1 !e 2 | isnottestedatall. |     | Thepredicatein!(e         |     | 1 e 2)e 1 is |
| --- | --- | --- | --- | --- | --- | ------------------------------------ | ------ | ----------------- | --- | ------------------------- | --- | ------------ |
|     |     |     |     |     |     | tested,andsucceedsbecauseofthefirste |        |                   |     | 1 ’sfailure,buttheoverall |     |              |
AgrammarGiswell-formedifalloftheexpressionsinitsexpres- resultisstillfailureduetothesecondinstanceofe 1 .
| sionsetE(G)arewell-formedinG. |     |     | Aswiththe*G |     | relation,the |     |     |     |     |     |     |     |
| ----------------------------- | --- | --- | ----------- | --- | ------------ | --- | --- | --- | --- | --- | --- | --- |
WF G setcanbecomputedbyiterationtoafixedpoint. Definition: Twoexpressionse 1 ande 2 aredisjointiftheysucceed
2)=0/.
|     |     |     |     |     |     | ondisjointsetsofinputstrings:M |     |     | G(e | 1)\M G(e |     |     |
| --- | --- | --- | --- | --- | --- | ------------------------------ | --- | --- | --- | -------- | --- | --- |
Lemma: AssumethatgrammarGiswell-formed,andthatallex-
(cid:3)oflengthnorless.Then
pressionsinE(G)handleallstringsx2V T Theorem: Achoiceexpressione 1=e 2 iscommutativeifitssubex-
theexpressionsinE(G)alsohandleallstringsoflengthn+1. pressionsaredisjoint.
Proof: Byinductionoverthestepcountsof)G . Theinteresting Proof:Ifeithere 1 ore 2 failsonastringx,itdoesnotmatterwhich
casesareasfollows: istestedfirst.Theonlywaythelanguagecanbeaffectedbychang-

ingtheirorderisife ande bothsucceedonxandconsumedif- Wefirst add three special nonterminals, T, Z, and F, with corre-
|     |     |     | 1   | 2   |     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
ferentamountsofinput.Disjointnessprecludesthispossibility. spondingrulesasfollows. ThenonterminalT matchesanysingle
|     |     |     |     |     |     |     |     |     | terminal,andhasthedefinition‘T |     |     | <- .’ inconcretePEGsyntax, |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------------------------ | --- | --- | -------------------------- | --- | --- |
AlthoughtheresultsfromSection3.4implythatdisjointnessisun- beforedesugaring. ThenonterminalZ matchesandconsumesany
decidableingeneral,itiseasyto“force”achoiceexpressiontobe inputstring;toavoidintroducingrepetitionoperators,wedefineit
disjointviathefollowingsimpletransformation: Z TZ=e . ThenonterminalF alwaysfails;toavoidusingpredi-
cateswedefineitF ZT.
| Theorem:e |     | 1=e 2(cid:16)e | 1=!e | e 2(cid:16)!e | e 2=e | ,andthelattertwoequiv- |     |     |     |     |     |     |     |     |
| --------- | --- | -------------- | ---- | ------------- | ----- | ---------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
|           |     |                |      | 1             | 1     | 1                      |     |     |     |     |     |     |     |     |
alentchoiceexpressionsaredisjoint. We define a function f recursively as follows, to convert expres-
sionsinouroriginalgrammarGintoourfirstnormalform:
Proof:Trivial,bycaseanalysis.
|     |     |     |     |     |     |     |     |     | 1. f(e)=eife2fe |     | g[V N[V | .   |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --------------- | --- | ------- | --- | --- | --- |
T
4 ReductionsonPEGs 2. f(e 1 e 2)=AB,addingA  f(e 1)andB  f(e 2)toR 1 .
|     |              |     |         |         |     |          |      |            | 3. f(e 1=e           | 2)=A=!A | f(e 2),addingA  |     | f(e 1)toR 1 | .   |
| --- | ------------ | --- | ------- | ------- | --- | -------- | ---- | ---------- | -------------------- | ------- | --------------- | --- | ----------- | --- |
| In  | this section | we  | present | methods | of  | reducing | PEGs | to simpler |                      |         |                 |     |             |     |
|     |              |     |         |         |     |          |      |            | 4. f(!e)=!A,addingA  |         | f(e)toR         | 1 . |             |     |
formsthatmaybemoreusefulforimplementationoreasiertorea-
so n a b o u t fo rm a l l y . F i r st w ed e sc r i b eh o w t o e lim in a te re pe t it io n 0;V
|                                                       |            |             |              |         |           |           |          |               | Definition:                | The stage | 1 grammar                        | G 1 of | G is (V N          | T;R 1;e S1), |
| ----------------------------------------------------- | ---------- | ----------- | ------------ | ------- | --------- | --------- | -------- | ------------- | -------------------------- | --------- | -------------------------------- | ------ | ------------------ | ------------ |
| a n                                                   | d p re d i | ca te o p e | r a to r s , | th en w | e s h o w | h o w P E | G sc a n | be m a p p ed |                            |           |                                  |        |                    |              |
|                                                       |            |             |              |         |           |           |          |               | wheree S1=f(e              | S),R      | 1=fA f(e)jA e2Rg[fnewdefinitions |        |                    |              |
| intothemuchmorerestrictiveTS/TDPLandgTS/GTDPLsystems. |            |             |              |         |           |           |          |               |                            |           |                                  | 0      |                    |              |
|                                                       |            |             |              |         |           |           |          |               | resultingfromapplicationof |           | fg,andV                          | N =V   | N[fnewnonterminals |              |
|                                                       |            |             |              |         |           |           |          |               | resultingfromapplicationof |           | fg.                              |        |                    |              |
4.1 EliminatingRepetitionOperators
|     |     |     |     |     |     |     |     |     | Lemma:Foranyexpressione, |     | f(e)(cid:16)G1 | e.  |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | -------------- | --- | --- | --- |
AsinCFGs,repetitionexpressionscanbeeliminatedfromaPEG
|              |     |                                |     |     |     |     |               |     | Proof: Bystructuralinductionovere.         |     |     | Theonlyinterestingcaseis |                |              |
| ------------ | --- | ------------------------------ | --- | --- | --- | --- | ------------- | --- | ------------------------------------------ | --- | --- | ------------------------ | -------------- | ------------ |
| byconverting |     | themintorecursivenonterminals. |     |     |     |     | UnlikeinCFGs, |     |                                            |     |     |                          |                |              |
|              |     |                                |     |     |     |     |               |     | forchoiceexpressions,whichusestheidentitye |     |     |                          | 1=e 2(cid:16)e | 1=!e 1 e 2 . |
thesubstitutenonterminalinaPEGmustberight-recursive.
|                                                      |     |                |     |            |          |        |            |        | Theorem:           | G 1(cid:16)G, | allsequenceandpredicateexpressionsinthe    |     |     |     |
| ---------------------------------------------------- | --- | -------------- | --- | ---------- | -------- | ------ | ---------- | ------ | ------------------ | ------------- | ------------------------------------------ | --- | --- | --- |
| Theorem:                                             |     | Any repetition |     | expression | e(cid:3) | can be | eliminated | by re- |                    |               |                                            |     |     |     |
|                                                      |     |                |     |            |          |        |            |        | expressionsetofG   |               | 1 containonlynonterminalsastheirsubexpres- |     |     |     |
| placingitwithanewnonterminalAwiththedefinitionA eA=e |     |                |     |            |          |        |            | .      |                    |               |                                            |     |     |     |
|                                                      |     |                |     |            |          |        |            |        | sions,andallchoice |               | expressionsaredisjoint.                    |     |     |     |
Proof:Byinductiononthelengthoftheinputstring.
|          |     |                                               |     |     |     |     |     |     | Proof:Directfromtheconstructionof |     |     | f.  |     |     |
| -------- | --- | --------------------------------------------- | --- | --- | --- | --- | --- | --- | --------------------------------- | --- | --- | --- | --- | --- |
| Theorem: |     | ForanyPEGG,anequivalentrepetition-freegrammar |     |     |     |     |     |     |                                   |     |     |     |     |     |
4.2.2 Stage2
G’canbecreated.
|     |     |     |     |     |     |     |     |     | We now | rewrite the | stage 1 grammar | G   | into another | equiva- |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------ | ----------- | --------------- | --- | ------------ | ------- |
Proof: SimplyeliminateallrepetitionexpressionsthroughoutG’s 1
|                                           |     |     |     |     |     |     |     |     | lentgrammarG                               | 2=(V                                 | 0;V T;R 2;e | S2),inwhichallnonterminalsei- |              |     |
| ----------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | ------------------------------------------ | ------------------------------------ | ----------- | ----------------------------- | ------------ | --- |
| nonterminaldefinitionsandstartexpression. |     |     |     |     |     |     |     |     |                                            |                                      | N           |                               |              |     |
|                                           |     |     |     |     |     |     |     |     | thersucceedandconsumeanonemptyinputprefix, |                                      |             |                               | orfail:      | 8A2 |
|                                           |     |     |     |     |     |     |     |     | V 0(A6*G2                                  | 0). Thistransformationisanalogoustoe |             |                               | -reductionon |     |
N
4.2 EliminatingPredicates CFGs,thoughthedetailsaredifferentduetopredicates.
|     |     |     |     |     |     |     |     |     | Weusetwofunctionsg |     | andg | ,to“split”expressionsintoe |     | -only |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------------ | --- | ---- | -------------------------- | --- | ----- |
In this section we show how to eliminate all predicate operators 0 1
|                           |     |     |     |     |                             |     |     |     | ande -freeparts,respectively. |     | Thee | -onlypartg | 0(e)ofanexpres- |     |
| ------------------------- | --- | --- | --- | --- | --------------------------- | --- | --- | --- | ----------------------------- | --- | ---- | ---------- | --------------- | --- |
| fromanywell-formedgrammar |     |     |     |     | whoselanguagedoesnotinclude |     |     |     |                               |     |      |            |                 |     |
sioneisanexpressionthatyieldsthesameresultaseonallinput
| the | empty | string. | The restriction |     | to grammars |     | that do | not accept |     |     |     |     |     |     |
| --- | ----- | ------- | --------------- | --- | ----------- | --- | ------- | ---------- | --- | --- | --- | --- | --- | --- |
stringsforwhichesucceedswithoutconsuminganyinput,andfails
theemptystringisaminorbutunavoidableproblem:wewillshow
|     |     |     |     |     |     |     |     |     | otherwise.Thee | -freepartg | 1(e)ofelikewiseyieldsthesameresult |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | -------------- | ---------- | ---------------------------------- | --- | --- | --- |
laterthatitisimpossibleforapredicate-freegrammartoacceptthe
aseonallinputsforwhichesucceedsandconsumesatleastone
emptystringwithoutacceptingallinputstrings.
terminal,andfailsotherwise.
| Given  | awell-formed,                                    |              | repetition-free |               | grammar    |                    | G=(V       | N;V T;R;e S)        |                |                       |     |     |     |     |
| ------ | ------------------------------------------------ | ------------ | --------------- | ------------- | ---------- | ------------------ | ---------- | ------------------- | -------------- | --------------------- | --- | --- | --- | --- |
|        | e                                                |              |                 |               |            |                    |            |                     | Wefirstdefineg | recursivelyasfollows: |     |     |     |     |
| where  |                                                  | 62 L(G),     | we will         | create        | an         | equivalent         | grammar    | G0 =                |                | 0                     |     |     |     |     |
| ( V    | 0 ; V T ; R                                      | 0 ;e 0 ) t h | at is w         | el l -f o r m | ed , r e p | e t i ti o n - fre | e , a n    | d p re d ic at e -  |                |                       |     |     |     |     |
|        | N                                                | S            |                 |               |            |                    |            |                     | 1. g 0(e       | )=e .                 |     |     |     |     |
| fr e   | e . T h i s                                      | p ro ce s    | s oc cu rs      | i n t h r ee  | n o r m    | a l i z a ti o n   | st a g e s | . I n th e fi r s t |                |                       |     |     |     |     |
| stage, | werewritethegrammarsothatsequenceandpredicateex- |              |                 |               |            |                    |            |                     | 2. g 0(a)=F.   |                       |     |     |     |     |
pressionsonlycontainnonterminalsandchoiceexpressionsaredis-
|     |     |     |     |     |     |     |     |     | 3. g 0(A)=g | 0(R | G(A)). |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ----------- | --- | ------ | --- | --- | --- |
joint. Inthesecondstage,wefurtherrewritethegrammarsothat
nonterminals never succeed without consuming any input. In the 4. g 0(AB)=g 0(A)g 0(B)ifA*0,otherwiseg 0(AB)=F.
thirdstagewefinallyeliminatepredicates.
|     |     |     |     |     |     |     |     |     | 5. g 0(e         | 1=e 2)=g | 0(e 1)=g 0(e 2). |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ---------------- | -------- | ---------------- | --- | --- | --- |
|     |     |     |     |     |     |     |     |     | 6. g 0(!A)=!(A=g |          | 0(A)).           |     |     |     |
4.2.1 Stage1
|     |     |     |     |     |     |     |     |     | Lemma:Thefunctiong |     | 0 terminatesifGiswell-formed. |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | ------------------ | --- | ----------------------------- | --- | --- | --- |
InthisstagewerewritetheexistingdefinitionsinRandtheoriginal
startexpressione S ,addingsomenewnonterminalsandcorrespond- Proof:BystructuralinductionovertheWF G relation.Termination
0,R
ingdefinitionsintheprocess,toproduceV N 1 ,ande S1 . reliesong 0(AB)notrecursivelyinvokingg 0(B)ifA6*0.

Wenowdefinethefunctiong primitive-recursivelyasfollows: Proof: Ifesucceeds, thenthe(Z=e )alsosucceedsandconsumes
1
|     |     |     |     |     |     |     | theentireremaininginput. |     |     |     | (ThenonterminalZ |     | aloneisnotsuffi- |     |
| --- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | --- | --- | ---------------- | --- | ---------------- | --- |
1. g 1(e )=F. cient because it was rewritten in stage 2 to be e -free.) Sinceany
|     |     |     |     |     |     |     | nonterminalC |     | is e -free, | the | overall | expression | willtherefore | fail. |
| --- | --- | --- | --- | --- | --- | --- | ------------ | --- | ----------- | --- | ------- | ---------- | ------------- | ----- |
2. g 1(a)=a.
|     |     |     |     |     |     |     | Ifefails,however,then(e(Z=e |     |     |     | )=e | )succeedswithoutconsuming |     |     |
| --- | --- | --- | --- | --- | --- | --- | --------------------------- | --- | --- | --- | --- | ------------------------- | --- | --- |
3. g 1(A)=A. anything,makingtheoverallexpressionbehaveaccordingtoC.
| 4. g 1(AB)=g | 0(A)B=Ag | 0(B)=AB. |     |     |     |     |     |     |          |          |      |           |            |          |
| ------------ | -------- | -------- | --- | --- | --- | --- | --- | --- | -------- | -------- | ---- | --------- | ---------- | -------- |
|              |          |          |     |     |     |     | We  | now | define a | function | h to | eliminate | predicates | from e - |
0
5. g 1(e 1=e 2)=g 1(e 1)=g 1(e 2). producingexpressionsresultingfromtheg ordfunctions:
0
| 6. g 1(!e)=F. |     |     |     |     |     |     |     | 0(e |     |     |     |     |     |     |
| ------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
1. h ;C)=C.
0;V
| Definition: | The stage | 2 grammar | G 2 | is (V | T;R 2;e | S2), where | 2.  | h 0(F;C)=F. |     |     |     |     |     |     |
| ----------- | --------- | --------- | --- | ----- | ------- | ---------- | --- | ----------- | --- | --- | --- | --- | --- | --- |
N
| R 2 = f A      |   g 1 ( e ) j A         | e2 R 1  | g , an d e | S 2 = g 1 (e  | S 1 ) =   | g 0 (e S 1 ) . W e |     |       |                |     |             |     |             |     |
| -------------- | ----------------------- | ------- | ---------- | ------------- | --------- | ------------------ | --- | ----- | -------------- | --- | ----------- | --- | ----------- | --- |
|                |                         |         |            |               |           |                    | 3.  | h 0(e | 1 e 2;C)=n(n(h | 0(e | 1;C);C)=n(h | 0(e | 2;C);C);C). |     |
| ef fe cti v el | y s p li t a l l of the | no n te | r m in al  | d efi n it io | n s i n R | 1 , re t a i n ing |     |       |                |     |             |     |             |     |
onlythee
-freepartsinthedefinitionsofR 2 ,whilesubstitutingthe 4. h 0(e 1=e 2;C)=h 0(e 1;C)=h 0(e 2;C).
correspondinge
-onlypartsatthepointswherethesenonterminals 5. h 0(!(B=e);C)=n(B=h 0(e;C);C).
| arereferencedinordertopreservetheoriginalbehavior. |          |     |                              |     |     | Thereare |     |                          |     |     |     |             |     |     |
| -------------------------------------------------- | -------- | --- | ---------------------------- | --- | --- | -------- | --- | ------------------------ | --- | --- | --- | ----------- | --- | --- |
|                                                    |          |     |                              |     |     |          | 6.  | h 0(!(A(B=e));C)=n(A(B=h |     |     |     | 0(e;C));C). |     |     |
| onlytwosuchpoints:                                 | case6ofg |     | 0 ,wherewerewritetheoperands |     |     |          |     |                          |     |     |     |             |     |     |
,fore
| ofpredicateexpressions,andcase4ofg |     |     |     | 1   | -freesequences. |     |        |     |              |            |     |        |             |          |
| ---------------------------------- | --- | --- | --- | --- | --------------- | --- | ------ | --- | ------------ | ---------- | --- | ------ | ----------- | -------- |
|                                    |     |     |     |     |                 |     | Lemma: |     | If e=g 0(e0) | or e=d(A;g |     | 0(e0)) | and e0 2E(G | 2), then |
h 0(e;C)isapredicate-freeexpressionequivalenttoeC.
Wesaythatthesplittinginvariantholdsifthefollowingistrue:
(cid:15) I f ( e ; x ) ) + e , the n (g ( e );x ) ) + e a n d ( g ( e ); x) ) + f . P r o o f : B y s t r u c tu r a l i nd u c t i o n o v e r e . C a se 5 h a n d l e s p r e d i c a t e s
|     | G1  | 0   | G   |     | 1   | G   |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
2 2 r e s u l t i n g d ir e c t l y f r o m g , w h i c h a l w a y s h a v e th e f o r m ! ( B = e ) ,
|     | +   |     |     |     |     | +   |     |     |     | 0   |     |     |     | 1   |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
(cid:15) I f ( e ; x ) ) y fo r j y j > 0 , t he n ( g 0( e ) ; x ) ) f a n d w h e r e e is l i k e w i s e a n e x p r e s s i o n r e s u l t in g f r om g . C a s e 6 s i m -
|     | G1  |     |     |     |     | G 2 |     | 1   |     |     |     |     | 0   |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
(g 1(e);x))+ y. i l ar l y ha nd les t h e s i tu a ti o n e = d ( A ; g ( e 0 )) . C a s e3 r e w r ite s a se -
|           | G2  |           |     |     |           |     |     |          |                |               |           | 0           |              |                    |
| --------- | --- | --------- | --- | --- | --------- | --- | --- | -------- | -------------- | ------------- | --------- | ----------- | ------------ | ------------------ |
|           |     |           |     |     |           |     | q u | e n ce e | e us i n g t h | e n o t -p re | dic a t e | a n a l o g | o f D e M or | g a n ’s L aw : if |
| If(e;x))+ |     | 0(e);x))+ |     |     | 1(e);x))+ |     |     | 1        | 2              |               |           |             |              |                    |
(cid:15) f,then(g f and(g f. e ande a ree -onlyexpressions,thene e 2(cid:16)!(!e 1=!e 2). Wecan-
|     | G1  |     |     | G2  |     | G2  | 1             | 2   |     |          |           | 1   |              |          |
| --- | --- | --- | --- | --- | --- | --- | ------------- | --- | --- | -------- | --------- | --- | ------------ | -------- |
|     |     |     |     |     |     |     | notsimplyuseh |     | 0(e | e 2;C)=h | 0(e 1;C)h | 0(e | 2;C)becauseh | 0(e 1;C) |
Lemma: Assume that the splitting invariant holds for all input 1
|            |                   |      |               |     |           |           | consumesinputifCsucceeds,whichwouldcausetheh |     |     |     |     |     |     | 0(e 2;C)part |
| ---------- | ----------------- | ---- | ------------- | --- | --------- | --------- | -------------------------------------------- | --- | --- | --- | --- | --- | --- | ------------ |
| strings of | length n or less. | Then | the splitting |     | invariant | holds for |                                              |     |     |     |     |     |     |              |
tostartatthewrongposition.
stringsoflengthn+1.
|                                         |     |     |     |     |        |     | Wenowdefineacorrespondingfunctionh |     |                                               |     |     |     | toeliminatepredicates |     |
| --------------------------------------- | --- | --- | --- | --- | ------ | --- | ---------------------------------- | --- | --------------------------------------------- | --- | --- | --- | --------------------- | --- |
| Proof:Byinductionoverthestepcountsof)G1 |     |     |     |     | and)G2 | .   |                                    |     |                                               |     |     | 1   |                       |     |
|                                         |     |     |     |     |        |     | fromthee                           |     | -freeexpressionsgeneratedbythestage2functiong |     |     |     |                       | :   |
1
| Theorem:     | G iswell-formedandequivalenttoG,andforallnon- |     |     |     |     |     |     |                     |     |     |     |     |     |     |
| ------------ | --------------------------------------------- | --- | --- | --- | --- | --- | --- | ------------------- | --- | --- | --- | --- | --- | --- |
|              | 2                                             |     |     |     |     |     | 1.  | h 1(e)=e,ife2fa;Ag. |     |     |     |     |     |     |
| terminalsA2V | ,A6*G2                                        | 0.  |     |     |     |     |     |                     |     |     |     |     |     |     |
N
2. h 1(AB)=AB.
Proof: Adirectconsequenceofthesplittinginvariantandthefact
|     |     |     |     |     |     |     | 3.  | h 1(e | 1 B)=h 0(e | 1;B),ife | 1 isnotanonterminal. |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | ----- | ---------- | -------- | -------------------- | --- | --- | --- |
thatGiswell-formed.
|       |        |     |     |     |     |     | 4.  | h 1(Ae | 2)=h 0(d(A;e | 2);A),ife |         | 2 isnotanonterminal. |     |     |
| ----- | ------ | --- | --- | --- | --- | --- | --- | ------ | ------------ | --------- | ------- | -------------------- | --- | --- |
| 4.2.3 | Stage3 |     |     |     |     |     | 5.  | h 1(e  | 1=e 2)=h     | 1(e 1)=h  | 1(e 2). |                      |     |     |
|       |        |     |     |     |     |     |     |        |              | 0 02      |         |                      |     |     |
intothefinalgrammarG0=(V 0;V T;R0;e0 L e m m a : If e = g 1 ( e ) a n d e E (G 2),thenh 1(e)isapredicate-free
| FinallywerewriteG | 2                                               |     |     |     |     | ).  |       |            |                 |             |       |     |     |     |
| ----------------- | ----------------------------------------------- | --- | --- | --- | --- | --- | ----- | ---------- | --------------- | ----------- | ----- | --- | --- | --- |
|                   |                                                 |     |     |     | N   | S   | e x p | re ss io n | e q uiv a l e n | t t o e i n | G 2 . |     |     |     |
| Definition:       | Wedefineafunctiond,suchthatd(A;e)“distributes”a |     |     |     |     |     |       |            |                 |             |       |     |     |     |
nonterminalAintoane Proof: By structural induction over e. In case 3, we know from
|                 |     | -onlyexpressioneresultingfromthestage |     |     |     |     |               |             |                  |               | e              |                           |             |             |
| --------------- | --- | ------------------------------------- | --- | --- | --- | --- | ------------- | ----------- | ---------------- | ------------- | -------------- | ------------------------- | ----------- | ----------- |
|                 |     |                                       |     |     |     |     | thedefinition |             | ofg 1            | thate 1       | is an          | -only expressionresulting |             | from        |
| 2functiong      | 0 : |                                       |     |     |     |     |               |             |                  |               |                |                           |             |             |
|                 |     |                                       |     |     |     |     | g 0 ,         | so we       | use the function |               | h 0 to combine |                           | it with the | subsequent  |
|                 |     |                                       |     |     |     |     | (e -free)     | nonterminal |                  | and eliminate |                | predicates                | from e      | . Case 4 is |
| d(A;e)=e,ife2fe |     |                                       |     |     |     |     |               |             |                  |               |                |                           |             | 1           |
1. ;Fg. similar,exceptthatwemustfirstmovee totheleftofAusingthe
2
dfunctionbeforeapplyingthepredicatetransformation.
| 2. d(A;e | 1 e 2)=d(A;e | 1)d(A;e | 2). |     |     |     |     |     |     |     |     |     |     |     |
| -------- | ------------ | ------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
3. d(A;e 1=e 2)=d(A;e 1)=d(A;e 2). Definition: The predicate-reduced grammar G0 of G is
4. d(A;!e)=!(Ae). (V 0;V T;R0;e0 ), where V 0 is the set of nonterminals produced
|     |                |     |     |     |     |     | N   |       | S    | N    |        |       |           |       |
| --- | -------------- | --- | --- | --- | --- | --- | --- | ----- | ---- | ---- | ------ | ----- | --------- | ----- |
|     |                |     |     |     |     |     |     |       | R0   |      |        |       |           | e0    |
|     |                |     |     |     |     |     | in  | stage | 1, = | fA   | h 1(e) | j A   | e 2 R 2g, | and = |
|     | 0(e0)ande02E(G |     |     |     |     |     |     |       |      |      |        |       |           | S     |
Lemma: Ife=g 2),thenAe(cid:16)d(A;e)A. That h 1(g 1(e S1))=h 0(g 0(e S1);T).
is,wecanused(A;e)tomoveeleftwardacrossanonterminalref-
G0
erenceinasequenceexpression. Theorem: is well-formed, repetition-free, predicate-free, and
equivalenttoG.
Proof:StructuralinductiononeandtheidentitiesinSection3.7.
G0
|                                 |     |     |     |     |     |     | Proof: |     | is repetition-free |     | because | G is | repetition-free | and we |
| ------------------------------- | --- | --- | --- | --- | --- | --- | ------ | --- | ------------------ | --- | ------- | ---- | --------------- | ------ |
| Nowdefineafunctionn(e;C)=(e(Z=e |     |     |     | )=e |     |     |        |     |                    |     |         |      |                 |        |
)C. neverintroducedanyrepetitionoperators.Fromthepreviousresult,
eachnonterminalA2V0
N isequivalenttothecorrespondingnonter-
Lemma:Ifeisane
-onlyexpressioninG 2 ,then!eC(cid:16)n(e;C). minalinthestage2grammar. Bythesameresult,theh 1(g 1(e S1))

partofthenewstartexpressione0 isequivalenttothee -freepartof Theorem: Anypredicate-freePEGG=(V N;V T;R;e S)canbere-
S
theoriginalstartexpressione .Theh 0(g 0(e S1);T)inthenewstart ducedtoanequivalentTDPLgrammarG0=V 0;V T;R0;S).
|     | S   |     |     |     |     |     | N   |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
expressionsucceedsandconsumesexactlyoneterminalwhenever
theinputstringisnonemptyandthee -onlypartoftheoriginalstart Proof: FirstweaddanewnonterminalS withdefinitionS e ,
S
expressione succeeds. Finally,sincewemadetheassumptionat representing the original start expression. We then add two non-
S
thestartthattheoriginalgrammardoesnotaccepttheemptystring, terminalsE andF withdefinitionsE e andF  f respectively.
the transformed grammar behaves identically for this degenerate Finally,werewriteeachdefinitionthatdoesnotconformtooneof
case. Sincetheacceptanceofastringintothelanguageofagram- theTDPLformsaboveusingthefollowingrules:
maronlydependsonthesuccessorfailureofthestartexpression,
and notonhowmuchoftheinputthestart expressionconsumes, A B 7(cid:0)! A BE=F
thenewgrammarG0acceptsexactlythesamestringsasG. A e e 7(cid:0)! A BC=F
1 2
B e
1
C e
2
| 4.2.4 TheEmptyStringLimitation |     |     |     | A e | 1=e | 7(cid:0)! A BE=C |     |     |
| ------------------------------ | --- | --- | --- | --- | --- | ---------------- | --- | --- |
2
B e
1
| Toshow thatwehavenohopeofavoidingtherestrictionthatthe |     |     |     |     |     | C e |     |     |
| ------------------------------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- |
2
original grammar cannot acceptthe empty input string, we prove A e(cid:3) 7(cid:0)! A BA=E
that any predicate-free grammar cannot accept the empty input B e
stringwithoutacceptingallinputstrings.
AhoandUllmandefinean“extendedTDPL”notation[3]equiva-
Lemma: AssumethatGisapredicate-freegrammar,andthatfor lentinexpressivenesstorepetition-free,predicate-freePEGs,with
any expression e and input x of length n or less, (e;e ))+ e iff reductionrulesalmostidenticaltothoseabove.
| (e;x))+e .Thenthesameholdsforinputstringsoflengthn+1. |     |     |     |     |     |     |     |     |
| ----------------------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- |
4.4 ReductiontogTS/GTDPL
| Proof:Byinductionoverstepcountsin)G |     | .   |                       |     |     |               |       |              |
| ----------------------------------- | --- | --- | --------------------- | --- | --- | ------------- | ----- | ------------ |
|                                     |     |     | Birman’s “generalized |     | TS” | (gTS) system, | named | “generalized |
Theorem:Inarepetition-freegrammarG,anexpressionematches
TDPL”(GTDPL)byAhoandUllman,issimilartoTDPL,butuses
theemptystringiffitmatchesallinputstringsandproducesonlye
slightlydifferentbasicruleformsthateffectivelyprovidethefunc-
| results.Inconsequence,e | 2L(G)impliesL(G)=V(cid:3). |     |     |     |     |     |     |     |
| ----------------------- | -------------------------- | --- | --- | --- | --- | --- | --- | --- |
T tionalityofpredicatesinPEGs.
Proof:Byinductionoverstringlength.
|     |     |     | Definition: | A GTDPL      | grammar | is a       | PEG G=(V    | N;V T;R;S) in |
| --- | --- | --- | ----------- | ------------ | ------- | ---------- | ----------- | ------------- |
|     |     |     | which S is  | anonterminal | and     | all of the | definitions | in R have one |
WecouldworkaroundtheemptystringlimitationbydefiningPEGs
ofthefollowingforms:
torequireallrecognizedstringstoincludeadesignatedendmarker
terminal,asBirmandoesintheoriginalTSandgTSsystems[5].
A e
|     |     |     | 1.              | .   |     |     |     |     |
| --- | --- | --- | --------------- | --- | --- | --- | --- | --- |
|     |     |     | 2. A a,wherea2V |     | .   |     |     |     |
T
4.3 ReductiontoTS/TDPL
|     |     |     | 3. A  | f,where | f (cid:17)!e . |     |     |     |
| --- | --- | --- | ----- | ------- | -------------- | --- | --- | --- |
Wecanreduceanypredicate-freePEGtoaninstanceofBirman’s 4. A B[C;D],whereB[C;D](cid:17)BC=!BD,andB;C;D2V .
N
TSsystem[4,5],renamed“Top-DownParsingLanguage”(TDPL)
byAhoandUllman[3]. Wewillusethelattertermforitsdescrip- Theorem: Any PEG G=(V N;V T;R;e S) can be reduced to an
tiveness. TDPL uses a set of grammar-like definitions, but these equivalentGTDPLgrammarG0=V 0;V T;R0;S).
N
definitionshaveonlyafewfixedformsinplaceofopen-endedhier-
archicalparsingexpressions. WecanviewTDPLasthePEGana- Proof: FirstweaddthedefinitionsS e ,E e ,andF  f,as
S
log of Chomsky Normal Form (CNF) for context-free grammars. above for TDPL. Then we rewrite all non-conforming definitions
Instead of defining TDPL “from the ground up” as Birman does, usingthefollowingtransformations:
wesimplydefineitasarestrictedformofPEG.
|     |     |     |     | A B |     | 7(cid:0)! A B[E;F] |     |     |
| --- | --- | --- | --- | --- | --- | ------------------ | --- | --- |
Definition: ATDPLgrammarisaPEGG=(V N;V T;R;S)inwhich A e e 7(cid:0)! A B[C;F]
1 2
| SisanonterminalinV | andallofthedefinitionsinRhaveoneof |     |     |     |     | B e |     |     |
| ------------------ | ---------------------------------- | --- | --- | --- | --- | --- | --- | --- |
|                    | N                                  |     |     |     |     |     | 1   |     |
| thefollowingforms: |                                    |     |     |     |     | C e |     |     |
2
|     |     |     |     | A e | 1=e | 7(cid:0)! A B[E;C] |     |     |
| --- | --- | --- | --- | --- | --- | ------------------ | --- | --- |
2
| A e  |     |     |     |     |     | B e |     |     |
| ---- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1. . |     |     |     |     |     |     | 1   |     |
C   e
| 2. A a,wherea2V | .   |     |     |            |     |     | 2       |     |
| --------------- | --- | --- | --- | ---------- | --- | --- | ------- | --- |
|                 | T   |     |     | A e(cid:3) |     | A   | B [A;E] |     |
7(cid:0)!
| 3. A  f,where          | f (cid:17)!e . |     |     |      |     | B e                |     |     |
| ---------------------- | -------------- | --- | --- | ---- | --- | ------------------ | --- | --- |
| 4. A BC=D,whereB;C;D2V |                | .   |     | A !e |     | 7(cid:0)! A B[F;E] |     |     |
N
B e
| Thethirdform,A  | f,representingunconditionalfailure,iscon- |     |     |     |     |     |     |     |
| --------------- | ----------------------------------------- | --- | --- | --- | --- | --- | --- | --- |
4.4.1 ParsingPEGs
sidered“primitive”inTDPL,althoughwedefineithereintermsof
theparsingexpression!e
. Thefourthform,A BC=D,combines
the functions of nonterminals, sequencing, and choice. A TDPL Corollary: Itis possibletoconstructalinear-timeparserforany
grammarGisinterpretedaccordingtotheusual)G relation. PEGonareasonablerandom-accessmemorymachine.

Proof: Reduce the PEG to a GTDPL grammar and then use the work.Themajornewfeaturesofthepresentworkaretheextension
tabularparsingtechniquedescribedbyAhoandUllman[3]. tosupportgeneralparsingexpressionswithrepetitionandpredicate
operators,thestructuralanalysisandidentityresultsinSections3.5
In practice it is not necessary to reduce a PEG all the way to through3.7,andthepredicateeliminationprocedureinSection4.2.
TDPL or GTDPL form, though it is typically necessary at least Whileparsingexpressionscouldconceivablybetreatedmerelyas
toeliminaterepetitionoperators. Practicalmethodsforconstruct- “syntacticsugar”forGTDPLgrammars,itisnotclearthatthepred-
ingsuchlinear-timeparsersbothmanuallyandautomatically,par- icateeliminationtransformation,andhencethereductionfromGT-
ticularlyusingmodernfunctionalprogramminglanguagessuchas DPL to TDPL, could be accomplished without the use of more
Haskell[11],arediscussedinpriorwork[8,7]. generalexpression-like formsintheintermediate stages. Forthis
|     |     |     |     |     |     |     |     | reason it | appears | that PEGs | represent | a useful | formal | notation in |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ------- | --------- | --------- | -------- | ------ | ----------- |
4.4.2 EquivalenceofTDPLandGTDPL itsownright,complementarytotheminimalistTDPLandGTDPL
systems.
Theorem:Anywell-formedGTDPLgrammarthatdoesnotaccept
theemptystringcanbereducedtoanequivalentTDPLgrammar. Unfortunately it appears TDPL and GTDPL have not seen much
|     |     |     |     |     |     |     |     | practical | use, perhaps | in large | measure | because | they | were origi- |
| --- | --- | --- | --- | --- | --- | --- | --- | --------- | ------------ | -------- | ------- | ------- | ---- | ----------- |
Proof: TreatingtheoriginalGTDPLgrammarasarepetition-free nally developed and presented as formalmodels for certaintypes
PEG,firsteliminatepredicates(Section4.2),thenreducetheresult- oftop-downparsers,ratherthanasausefulsyntacticfoundationin
ingpredicate-freegrammartoTDPL(Section4.3). itsownright. Adams[1]usedTDPLinamodularlanguageproto-
|        |          |     |     |     |     |     |     | typingframework,however. |     |               | Inaddition,manypracticaltop-down |     |               |      |
| ------ | -------- | --- | --- | --- | --- | --- | --- | ------------------------ | --- | ------------- | -------------------------------- | --- | ------------- | ---- |
|        |          |     |     |     |     |     |     | parsing libraries        |     | and toolkits, | including                        | the | popular ANTLR | [21] |
| 5 Open | Problems |     |     |     |     |     |     |                          |     |               |                                  |     |               |      |
andthePARSECcombinatorlibraryforHaskell[15],provideback-
trackingcapabilitiesthatconformtothismodelinpractice,ifper-
| This section | briefly | outlines | some | promising | directions |     | for future |                      |     |                                         |     |     |     |     |
| ------------ | ------- | -------- | ---- | --------- | ---------- | --- | ---------- | -------------------- | --- | --------------------------------------- | --- | --- | --- | --- |
|              |         |          |      |           |            |     |            | hapsunintentionally. |     | Theseexistingsystemsgenerallyuse“naive” |     |     |     |     |
workonPEGsandrelatedsyntacticformalisms.
|     |     |     |     |     |     |     |     | backtracking | methods | that | risk exponential |     | runtime in | worst-case |
| --- | --- | --- | --- | --- | --- | --- | --- | ------------ | ------- | ---- | ---------------- | --- | ---------- | ---------- |
scenarios,butthesamefeaturescanbeimplementedinstrictlylin-
| Birman defined | a   | transformation |     | on gTS | that converts |     | loop fail- |     |     |     |     |     |     |     |
| -------------- | --- | -------------- | --- | ------ | ------------- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
eartimeusingamemoizing“packratparser”[8,7].
urescausedbygrammarcircularitiesintoordinaryrecognitionfail-
ures[5].ByextensionitispossibletoconvertanyPEGintoacom-
Thepositiveformofsyntacticpredicate(the“and-predicate”)was
| plete PEG. | It is probably |     | possible | to transform |     | any PEG | into an |     |     |     |     |     |     |     |
| ---------- | -------------- | --- | -------- | ------------ | --- | ------- | ------- | --- | --- | --- | --- | --- | --- | --- |
introducedbyParr[20]foruseinANTLR[21],andlaterincorpo-
equivalentwell-formedPEG,butthisconjectureisunverified;Bir-
|     |     |     |     |     |     |     |     | ratedintoJavaCCunderthename“syntacticlookahead”[16]. |     |     |     |     |     | The |
| --- | --- | --- | --- | --- | --- | --- | --- | ---------------------------------------------------- | --- | --- | --- | --- | --- | --- |
mandidnotdefineastructuralwell-formednesspropertyforgTS.
metafrontsystemincludesalimited,fixed-lookaheadformofsyn-
| Such a transformation |                | on  | PEGs          | is conceivable |           | despite        | the unde- |                                                         |     |     |     |     |     |     |
| --------------------- | -------------- | --- | ------------- | -------------- | --------- | -------------- | --------- | ------------------------------------------------------- | --- | --- | --- | --- | --- | --- |
|                       |                |     |               |                |           |                |           | tacticpredicatesundertheterms“attractors”and“traps”[6]. |     |     |     |     |     | The |
| cidability            | of a grammar’s |     | completeness, |                | since the | transformation |           |                                                         |     |     |     |     |     |     |
negativeformofsyntacticpredicate(the“not-predicate”)appearsto
worksessentiallybybuilding“run-time”circularitychecksintothe
benew, butitseffectcanbeachievedinpracticalparsingsystems
| grammar | instead | of trying | to decide | statically |     | at “compile-time” |     |     |     |     |     |     |     |     |
| ------- | ------- | --------- | --------- | ---------- | --- | ----------------- | --- | --- | --- | --- | --- | --- | --- | --- |
suchasANTLRandJavaCCusingsemanticpredicates[17].
whetheranycircularconditionsarereachable.
|     |     |     |     |     |     |     |     | Many extensions |     | and variations |     | of context-free | grammars | have |
| --- | --- | --- | --- | --- | --- | --- | --- | --------------- | --- | -------------- | --- | --------------- | -------- | ---- |
Perhapsofmorepracticalinterest,wewouldlikeausefulconserva-
beendeveloped,suchasindexedgrammars[2],W-grammars[28],
| tivealgorithmtodetermineifachoiceexpressione     |     |     |     |     |     | 1=e       | inagram- |                                          |       |                |     |          |                |            |
| ------------------------------------------------ | --- | --- | --- | --- | --- | --------- | -------- | ---------------------------------------- | ----- | -------------- | --- | -------- | -------------- | ---------- |
|                                                  |     |     |     |     |     | 2         |          | affix grammars                           | [13], | tree-adjoining |     | grammars | [12],          | minimalist |
| marisdefinitelydisjoint,andthereforecommutative. |     |     |     |     |     | Suchanal- |          |                                          |       |                |     |          |                |            |
|                                                  |     |     |     |     |     |           |          | grammars[24],andconjunctivegrammars[18]. |       |                |     |          | Mostoftheseex- |            |
gorithmwouldenableustoextendPEGsyntaxwithanunordered
|     |     |     |     |     |     |     |     | tensions | are motivated | by  | the requirements |     | of expressing | natural |
| --- | --- | --- | --- | --- | --- | --- | --- | -------- | ------------- | --- | ---------------- | --- | ------------- | ------- |
choiceoperator‘j’analogoustothechoiceoperatorusedinEBNF
languages,andallareatleastasdifficulttoparseasCFGs.
| syntaxforCFGs. |     | The‘j’operatorwouldbesemanticallyidentical |     |     |     |     |     |     |     |     |     |     |     |     |
| -------------- | --- | ------------------------------------------ | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
to‘=’,butwouldexpressthelanguagedesigner’sassertionthatthe
Sincemachine-orientedlanguagetranslatorsoftenneedtoprocess
alternativesaredisjointandthereforeorder-independent,andtools
largeinputsinlinearornear-lineartime,andthereappearstobeno
suchPEGanalyzersandPEG-basedparsergenerators[7]couldver- hopeofgeneralCFGparsinginmuchbetterthanO(n3)time[14],
ifytheseassertionsautomatically.
mostparsingalgorithmsformachine-orientedlanguagesfocuson
|              |         |     |                  |     |                          |     |     | handlingsubclasses |     | oftheCFGs. |     | Classicdeterministictop-down |     |     |
| ------------ | ------- | --- | ---------------- | --- | ------------------------ | --- | --- | ------------------ | --- | ---------- | --- | ---------------------------- | --- | --- |
| A final open | problem | is  | the relationship |     | and inter-convertibility |     |     |                    |     |            |     |                              |     |     |
andbottom-uptechniques[3]arewidelyused,buttheirlimitations
| of CFGs | and PEGs. | Birman | proved | that | TS and | gTS | can simu- |     |     |     |     |     |     |     |
| ------- | --------- | ------ | ------ | ---- | ------ | --- | --------- | --- | --- | --- | --- | --- | --- | --- |
arefrequentlyfeltbylanguagedesignersandimplementors.
| late any | deterministic | pushdown |     | automata | (DPDA) | [5], | implying |     |     |     |     |     |     |     |
| -------- | ------------- | -------- | --- | -------- | ------ | ---- | -------- | --- | --- | --- | --- | --- | --- | --- |
thatPEGscanexpressanydeterministicLR-classcontext-freelan-
ThesyntaxdefinitionformalismSDFincreasestheexpressiveness
| guage. There                            | is                                            | informal        | evidence, | however,    | that    | a much        | larger |                 |               |                |         |           |                  |             |
| --------------------------------------- | --------------------------------------------- | --------------- | --------- | ----------- | ------- | ------------- | ------ | --------------- | ------------- | -------------- | ------- | --------- | ---------------- | ----------- |
|                                         |                                               |                 |           |             |         |               |        | of CFGs         | with explicit | disambiguation |         | rules,    | and supports     | unified     |
| classofCFGsmightberecognizablewithPEGs, |                                               |                 |           |             |         | includingmany |        |                 |               |                |         |           |                  |             |
|                                         |                                               |                 |           |             |         |               |        | language        | descriptions  | by combining   |         | lexical   | and context-free | syn-        |
| CFGs for                                | which                                         | no conventional |           | linear-time | parsing | algorithm     | is     |                 |               |                |         |           |                  |             |
|                                         |                                               |                 |           |             |         |               |        | tax definitions | into          | a “two-level”  |         | formalism | [10]. The        | nondeter-   |
| known[7].                               | ItisnotevenprovenyetthatCFLsexistthatcannotbe |                 |           |             |         |               |        |                 |               |                |         |           |                  |             |
|                                         |                                               |                 |           |             |         |               |        | ministic        | linear-time   | NSLR(1)        | parsing | algorithm | [26]             | is powerful |
recognizedbyaPEG,thoughrecentworkinlowerboundsonthe
enoughtogenerate“scannerless”parsersfromunifiedsyntaxdefi-
| complexity | of general | CFG | parsing | [14] | and matrix | product | [23] |     |     |     |     |     |     |     |
| ---------- | ---------- | --- | ------- | ---- | ---------- | ------- | ---- | --- | --- | --- | --- | --- | --- | --- |
nitionswithouttreatinglexicalanalysisseparately[22],buttheal-
showsatleastthatgeneralCFGparsingisinherentlysuper-linear.
gorithmseverelyrestrictstheforminwhichsuchCFGscanbewrit-
ten.Othermachine-orientedsyntaxformalismsandtoolsuseCFGs
| 6 Related | Work |     |     |     |     |     |     |     |     |     |     |     |     |     |
| --------- | ---- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
extendedwithexplicitdisambiguationrulestoexpressbothlexical
andhierarchicalsyntax,supportingunifiedsyntaxdefinitionsmore
ThisworkisinspiredbyandheavilybasedonBirman’sTS/TDPL cleanly while giving up strictly linear-time parsing [21, 29, 27].
andgTS/GTDPLsystems[4,5,3]. The)G relationandthebasic Thesesystemsgraftrecognition-basedfunctionalityontogenerative
CFGs,resultingina“hybrid”generative/recognition-basedsyntac-
propertiesinSections3.3and3.4aredirectadaptationsofBirman’s

tic model. PEGs provide similar features in a simpler syntactic [11] SimonPeytonJonesandJohnHughes(editors). Haskell 98
foundationbyadoptingtherecognitionparadigmfromthestart. Report,1998. http://www.haskell.org.
|     |     |     |     |     |     |     | [12] Aravind | K.  | Joshi and | Yves | Schabes. | Tree-adjoining | gram- |
| --- | --- | --- | --- | --- | --- | --- | ------------ | --- | --------- | ---- | -------- | -------------- | ----- |
7 Conclusion
|                    |             |               |         |             |                |        | mars.              | HandbookofFormalLanguages,3:69–124,1997. |                |       |         |                      |       |
| ------------------ | ----------- | ------------- | ------- | ----------- | -------------- | ------ | ------------------ | ---------------------------------------- | -------------- | ----- | ------- | -------------------- | ----- |
|                    |             |               |         |             |                |        | [13] C.H.A.Koster. |                                          | Affixgrammars. |       |         | InJ.E.L.Peck,editor, | AL-   |
| Parsing expression |             | grammars      | provide | a powerful, | formally       | rigor- |                    |                                          |                |       |         |                      |       |
|                    |             |               |         |             |                |        | GOL                | 68 Implementation,                       |                | pages | 95–109, | Amsterdam,           | 1971. |
| ous, and           | efficiently | implementable |         | foundation  | for expressing | the    |                    |                                          |                |       |         |                      |       |
North-HollandPubl.Co.
| syntaxofmachine-orientedlanguagesthataredesigned |     |     |     |     |     | tobeun- |     |     |     |     |     |     |     |
| ------------------------------------------------ | --- | --- | --- | --- | --- | ------- | --- | --- | --- | --- | --- | --- | --- |
ambiguous. Because of their implicit longest-match recognition [14] LillianLee. Fastcontext-freegrammarparsingrequiresfast
capability coupled with explicit predicates, PEGs allow both the booleanmatrixmultiplication. JournaloftheACM,49(1):1–
| lexicalandhierarchicalsyntaxofalanguagetobedescribedinone |     |     |     |     |     |     | 15,2002. |     |     |     |     |     |     |
| --------------------------------------------------------- | --- | --- | --- | --- | --- | --- | -------- | --- | --- | --- | --- | --- | --- |
concisegrammar.TheexpressivenessofPEGsalsointroducesnew
|     |     |     |     |     |     |     | [15] Daan | Leijen. |     | Parsec, | a   | fast combinator | parser. |
| --- | --- | --- | --- | --- | --- | --- | --------- | ------- | --- | ------- | --- | --------------- | ------- |
syntaxdesignchoicesforfuturelanguages. Birman’sGTDPLsys- http://www.cs.uu.nl/˜daan.
temservesasanatural“normalform”towhichanyPEGcaneasily
|     |     |     |     |     |     |     | [16] Sun | Microsystems. |     | Java | compiler | compiler | (JavaCC). |
| --- | --- | --- | --- | --- | --- | --- | -------- | ------------- | --- | ---- | -------- | -------- | --------- |
bereduced.Withminorrestrictions,PEGscanberewrittentoelim-
https://javacc.dev.java.net/.
| inate predicates | and | reduced | to TDPL, | an  | even more | minimalist |     |     |     |     |     |     |     |
| ---------------- | --- | ------- | -------- | --- | --------- | ---------- | --- | --- | --- | --- | --- | --- | --- |
form. In consequence, we have shown TDPL and GTDPL to be [17] Sun Microsystems. JavaCC: LOOKAHEAD minitutorial.
essentially equivalent in recognition power. Finally, despite their https://javacc.dev.java.net/doc/lookahead.html.
| ability to | express | language | constructs | requiring | unlimited | looka- |                        |     |     |                      |     |     |              |
| ---------- | ------- | -------- | ---------- | --------- | --------- | ------ | ---------------------- | --- | --- | -------------------- | --- | --- | ------------ |
|            |         |          |            |           |           |        | [18] AlexanderOkhotin. |     |     | Conjunctivegrammars. |     |     | JournalofAu- |
headandbacktracking,allPEGsareparseableinlineartimewitha
suitabletabularormemoizingalgorithm. tomata,LanguagesandCombinatorics,6(4):519–535,2001.
|                 |         |          |         |                 |     |            | [19] International                     |     | Standards | Organization. |               | Syntactic         | metalan- |
| --------------- | ------- | -------- | ------- | --------------- | --- | ---------- | -------------------------------------- | --- | --------- | ------------- | ------------- | ----------------- | -------- |
| Acknowledgments |         |          |         |                 |     |            | guage—ExtendedBNF,1996.                |     |           |               | ISO/IEC14977. |                   |          |
|                 |         |          |         |                 |     |            | [20] TerenceJ.ParrandRussellW.Quong.   |     |           |               |               | Addingsemanticand |          |
| I would         | like to | thank my | advisor | Frans Kaashoek, |     | as well as |                                        |     |           |               |               |                   |          |
|                 |         |          |         |                 |     |            | syntacticpredicatestoLL(k)—pred-LL(k). |     |           |               |               | InProceedingsof   |          |
Franc¸oisPottier,RobertGrimm,TerenceParr,ArnarBirgisson,and
theInternationalConferenceonCompilerConstruction,Ed-
thePOPLreviewers,forvaluablefeedbackanddiscussionandfor
inburgh,Scotland,April1994.
pointingoutseveralerrorsintheoriginaldraft.
[21] TerenceJ.ParrandRussellW.Quong.ANTLR:APredicated-
8 References LL(k) parser generator. Software Practice and Experience,
25(7):789–810,1995.
[1] Stephen Robert Adams. Modular Grammars for Program- [22] Daniel J. Salomon and Gordon V. Cormack. Scannerless
ming Language Prototyping. PhD thesis, University of NSLR(1)parsingofprogramminglanguages. InProceedings
Southampton,1991. oftheACMSIGPLAN’89ConferenceonProgrammingLan-
|                  |     |                                       |     |     |     |     | guage | Design | and | Implementation |     | (PLDI), pages | 170–178, |
| ---------------- | --- | ------------------------------------- | --- | --- | --- | --- | ----- | ------ | --- | -------------- | --- | ------------- | -------- |
| [2] AlfredV.Aho. |     | Indexedgrammars—anextensionofcontext- |     |     |     |     |       |        |     |                |     |               |          |
Jul1989.
| freegrammars. |     | JournaloftheACM,15(4):647–671,October |     |     |     |     |     |     |     |     |     |     |     |
| ------------- | --- | ------------------------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
1968. [23] Amir Shpilka. Lower bounds for matrix product. In IEEE
SymposiumonFoundationsofComputerScience,pages358–
| [3] AlfredV.AhoandJeffreyD.Ullman. |     |     |     | TheTheoryofParsing, |     |     |     |     |     |     |     |     |     |
| ---------------------------------- | --- | --- | --- | ------------------- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
367,2001.
| Translation |     | and Compiling | -   | Vol. I: Parsing. |     | Prentice Hall, |     |     |     |     |     |     |     |
| ----------- | --- | ------------- | --- | ---------------- | --- | -------------- | --- | --- | --- | --- | --- | --- | --- |
EnglewoodCliffs,N.J.,1972. [24] EdwardStabler.Derivationalminimalism.LogicalAspectsof
ComputationalLinguistics,pages68–95,1997.
| [4] AlexanderBirman. |     | TheTMGRecognitionSchema. |     |     |     | PhDthe- |     |     |     |     |     |     |     |
| -------------------- | --- | ------------------------ | --- | --- | --- | ------- | --- | --- | --- | --- | --- | --- | --- |
sis,PrincetonUniversity,February1970. [25] Bjarne Stroustrup. The C++ Programming Language.
Addison-Wesley,3rdedition,June1997.
[5] AlexanderBirmanandJeffreyD.Ullman.Parsingalgorithms
withbacktrack.InformationandControl,23(1):1–34,August [26] Kuo-Chung Tai. Noncanonical SLR(1) grammars. ACM
| 1973. |     |     |     |     |     |     | Transactions |     | on  | Programming |     | Languages | and Systems, |
| ----- | --- | --- | --- | --- | --- | --- | ------------ | --- | --- | ----------- | --- | --------- | ------------ |
1(2):295–320,Oct1979.
| [6] Claus | Brabrand, | Michael | I. Schwartzbach, |     | and | Mads Vang- |     |     |     |     |     |     |     |
| --------- | --------- | ------- | ---------------- | --- | --- | ---------- | --- | --- | --- | --- | --- | --- | --- |
gaard. Themetafrontsystem:Extensibleparsingandtrans- [27] M.G.J.vandenBrand,J.Scheerder,J.J.Vinju,andE.Visser.
formation. In Third Workshop on Language Descriptions, DisambiguationfiltersforscannerlessgeneralizedLRparsers.
ToolsandApplications,Warsaw,Poland,April2003. InCompilerConstruction,2002.
[7] BryanFord.Packratparsing:apracticallinear-timealgorithm [28] A. van Wijngaarden, B.J. Mailloux, J.E.L. Peck, C.H.A.
withbacktracking.Master’sthesis,MassachusettsInstituteof Koster, M. Sintzoff, C.H. Lindsey, L.G.L.T. Meertens, and
Technology,Sep2002. R.G.Fisker. ReportonthealgorithmiclanguageALGOL68.
Numer.Math.,14:79–218,1969.
| [8] BryanFord. |     | Packratparsing: |     | Simple, | powerful, | lazy, linear |     |     |     |     |     |     |     |
| -------------- | --- | --------------- | --- | ------- | --------- | ------------ | --- | --- | --- | --- | --- | --- | --- |
time.InProceedingsofthe2002InternationalConferenceon [29] EelcoVisser.Afamilyofsyntaxdefinitionformalisms.Tech-
FunctionalProgramming,Oct2002. nicalReport P9706, ProgrammingResearchGroup, Univer-
sityofAmsterdam,1997.
| [9] Dick | Grune | and Ceriel | J.H.Jacobs. | Parsing | Techniques—A |     |     |     |     |     |     |     |     |
| -------- | ----- | ---------- | ----------- | ------- | ------------ | --- | --- | --- | --- | --- | --- | --- | --- |
PracticalGuide. EllisHorwood,Chichester,England,1990. [30] NiklausWirth. Whatcanwedoabouttheunnecessarydiver-
sityofnotationforsyntacticdescriptions.Communicationsof
| [10] J. Heering, |     | P. R. H. Hendriks, |     | P. Klint, | and J. | Rekers. The |     |     |     |     |     |     |     |
| ---------------- | --- | ------------------ | --- | --------- | ------ | ----------- | --- | --- | --- | --- | --- | --- | --- |
theACM,20(11):822–823,November1977.
| syntaxdefinitionformalismSDF—referencemanual—. |     |     |     |     |     | SIG- |     |     |     |     |     |     |     |
| ---------------------------------------------- | --- | --- | --- | --- | --- | ---- | --- | --- | --- | --- | --- | --- | --- |
PLANNotices,24(11):43–75,1989.