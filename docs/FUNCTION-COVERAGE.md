# Funksjonsdekning: Listi på egne premisser

**Tilleggskrav 14. november 2026:** Alle Trackers brukeroppgaver skal også
dekkes i Listi, på vår egen måte. Se [Tracker-kartet](TRACKER-COVERAGE.md)
for dokumentert tilbud, udekkede behov og akseptkrav. Dette erstatter ikke
Dolphin-kravet og er ikke en påstand om ferdig funksjonslikhet.

## Samlet status etter privat 0.9.0

Kartet inneholder 81 brukerbehov fra Dolphin 26.08.3 og relevante tillegg.
[Tracker-kartet](TRACKER-COVERAGE.md) supplerer med 40 behovsområder. Referansene
er dokument- og kildegransket, ikke kjørt som full praktisk sammenligning.

| Område | Levert og avgrenset verifikasjon | Viktigste restbehov |
|---|---|---|
| Historikk, N03 | 0.5.0: uavhengig Tilbake/Frem per panel, også virtuelle steder. [Bevis](NAVIGATION-0.5.0.md) | Faner/mappekort, flere vinduer og øktgjenoppretting er egne udekkede behov |
| Lagrede steder, N05/R08 | 0.6.0: navngitte adresser uten innloggingsdata. [Bevis](PLACES-0.6.0.md) | Oppdagelse, kontohåndtering og bredere nettverkskvalifisering |
| Søk, V08-V10/T15-T18 | 0.7.0 navnesøk og 0.8.0 levende BFS/lagrede kriterier med identitetskontroll. [Bevis](ACCEPTANCE-0.8.0.md) | Attributtbygger, innhold, flervolumvalg og Tracker-søkefiler gjenstår |
| Filhandlinger og Haiku-samspill | 0.9.0: lokal filutklippstavle begge veier med Tracker, identitetskontroll og trygg avbruddsflyt. [Bevis](ACCEPTANCE-0.9.0.md) | Virtuell utklippstavle, utvidede egenskaper/attributter, MIME-redigering/standardtilknytninger, lenker, maler, masseomdøping, sammenligning og konfliktbevisst angre |
| Visninger og arbeidsflater | To paneler, detalj-/ikonvisning og mappetrær finnes | Mappekort/arbeidssett, visningsvalg, gruppering, skjulte filer og bredere tilgjengelighetsprøver |
| Arkiver, filkilder og integrasjoner | Arkivlesing/uttrekk og SMB/SFTP/FTP har avgrensede syntetiske prøver | Arkivoppretting/redigering/passord, flere formater, ekte servervariasjon, nye protokoller, enheter, sky og VCS |

Tabellene nedenfor beholder det daterte 0.4.0-basiskartet, med eksplisitt merkede
senere radoppdateringer. De er ikke alle kontrollert på nytt på 0.9.0. Vi beregner
ikke en dekningsprosent fra ulikt store behov. Oppdater denne samlede statusen ved
hver kvalifisert leveranse, og behold utestede deler som åpne krav.


14. november 2026. Arbeidsgrunnlag, ikke en påstand om funksjonslikhet.

## Produktkravet

Vi skal ikke kopiere Dolphin, verken kode, utforming eller arbeidsflyt. Dolphin
brukes som referanse for hvilke oppgaver en komplett filbehandler må kunne løse.
Når referansen tilbyr en funksjon, skal Listi også dekke behovet, med vår egen
løsning og Haikus muligheter. Det er resultatet for brukeren som sammenlignes.
Tillegg og protokoller skal ikke skjules ved å telle bare grunnprogrammet.

Denne kartleggingen tilfører ingen Dolphin-kode, ressurser eller komponenter til
Listi. Universelle filbegreper og native Haiku-konvensjoner er fortsatt grunnlaget.
Et behov kan løses gjennom en god native integrasjon, men «bruk Terminal selv» er
ikke automatisk funksjonsdekning. Funksjonen må være tilgjengelig og prøvd som en
sammenhengende Listi-arbeidsflyt. Utsettelse betyr udekket behov, ikke at kravet
forsvinner. Omfanget skal utvides når flere relevante funksjoner blir identifisert.

## Fast referanse og etterprøvbarhet

- **Dolphin 26.08.3**, utgitt 5. november 2026 på [KDEs appside][apps]. Taggen
  `v26.08.3` peker til commit `2fdee206603fd3a21928984e9a3d404e82ae9890`.
- **KIO Extras, Dolphin Plugins og Ark 26.08.3** er separate referansekomponenter.
  **KIO 6.23.0** er den kontrollerte biblioteksreferansen, også Dolphins oppgitte
  minste KF6-versjon. Dette er ikke en påstand om en installert Linux-distribusjon.
- Google Drive-utvidelsen oppgir 26.08.3; KIO S3 oppgir 1.0.2. Andre annonserte
  tillegg er med i behovslisten, men krever egen versjonsfestet runtime-prøve.
- Håndboken i Dolphin-taggen har fortsatt dokumenthode «KDE Gear 22.04» fra 2022.
  Den brukes ikke alene som bevis på ny eller uendret 2026-atferd. Faktiske
  handlingsregistre i 26.08.3 og komponentenes bygg-/protokolldata supplerer den.
- Dolphin og tilleggene er **ikke kjørt i denne kartleggingen**. Kolonnen
  «Referanse» betyr dokumentert eller kildebekreftet tilbud, ikke bestått test.
- Listi-basis er **0.4.0, kode/pakke `93a0138`**, med installert binær og avgrenset
  testbevis i [akseptkvitteringen](ACCEPTANCE-0.4.0.md). Senere dokumentasjonscommits
  endrer ikke denne binæren. Ingen bygg eller tidligere tester er gjentatt her.

Referansekilder:

- **D1:** [versjonsfestet Dolphin-håndbok][handbook], seksjonsnavn angis i tabellen.
- **D2:** [hovedvinduets handlingsregister][actions]: historikk, faner, sesjon,
  søk, angre/gjør om, paneler og valgmodus.
- **D3:** [filvisningens handlingsregister][viewactions]: oppretting, filhandlinger,
  zoom, sortering, gruppering, skjulte filer og visningsvalg.
- **K1:** [KIO Extras 26.08.3][extras], [arkivprotokollenes egenskaper][archivejson].
- **K2:** [KIO 6.23.0 arbeidere][workers], HTTP/WebDAV- og FTP-metadata i kildekvitteringen.
- **A1:** [Ark 26.08.3 håndbok][ark]. Ark er en integrasjon, ikke Dolphins egen arkivmotor.
- **P1:** [Dolphin Plugins 26.08.3][plugins], med valgfrie moduler i byggdefinisjonen.
- **E1:** [Dolphins annonserte utvidelser][apps], [Google Drive][gdrive], [S3][s3].
- **AD:** [Dolphins administrative integrasjon][admin], avhengig av separat støtte.

URL-er, filhash og kildeidentitet er bevart i
[referansekvitteringen](reference/dolphin-26.08.3-sources.json).
Fullstendige referansefiler ligger bare i den private undersøkelsesrapporten,
ikke i Listis kildekode eller pakke.

## Les status riktig

- **V:** Den avgrensede Listi-flyten i raden er verifisert på kandidaten. Ikke
  generell likhet med alle varianter av referansefunksjonen.
- **D:** Delvis dekning. Den konkrete mangelen eller testbegrensningen er oppgitt.
- **U:** Kode finnes, men denne brukerflyten er ikke kvalifisert på 0.4.0.
- **M:** Mangler en tilsvarende Listi-funksjon. Flyten er et akseptkrav, ikke en test
  som er utført.

Beviskoder: **T** = ACCEPTANCE-0.4.0, 55 lokale og 96 virtuelle installerte tester;
**UI** = samme kvitterings faktiske UI-prøver; **O** = 9 native feilinjeksjoner;
**I** = 6 native bilde-/eierskapstester. **H** = historiske UI-prøver i
VERIFICATION.md, ikke omkjøring på 0.4.0. Kildehenvisninger uten disse kodene er
kodegjennomgang, ikke bestått runtime-test.

### Kildeindeks for Listi-vurderingene

Dette er kildegjennomgang av 0.4.0, ikke nye runtime-prøver:

- N01-N10: [Pane](../src/Pane.cpp), [FolderTree](../src/FolderTree.cpp) og
  [App](../src/App.cpp), navigasjon, Go-meny, hovedvindu og scripting.
- V01-V07/V15: [PaneModel](../src/PaneModel.cpp), [FileDetails](../src/FileDetails.cpp),
  [FileGrid](../src/FileGrid.cpp), [NameFilter](../src/NameFilter.h) og
  [Preferences](../src/Preferences.cpp). Søk må ikke utledes fra MatchesNameFilter.
- V08-V12/V16-V18: registrerte handlinger i App, innstillinger i Preferences og
  feltene i [Common](../src/Common.h). Det finnes ikke registrerte tilsvarende
  metadataredigerings-, gruppering- eller snarveikonfigurasjonsflyter i basiskartet.
  Søk er siden implementert i FileSearch, SearchWorker og SearchWindow, med
  SavedSearches for kriterier. Se SEARCH-0.8.0.md og akseptkvitteringen.
- V13-V14: [ImagePreview](../src/ImagePreview.cpp), [BitmapMailbox](../src/BitmapMailbox.h),
  FileGrid og tekstviseren i App. Bildeoversetterens eksistens er ikke formatbevis.
- F01-F19: [FileOperation](../src/FileOperation.cpp), [VirtualOperation](../src/VirtualOperation.cpp),
  [Trash](../src/Trash.cpp), [OperationQueue](../src/OperationQueue.cpp),
  [InlineAction](../src/InlineAction.cpp), [SelfTest](../src/SelfTest.cpp) og
  [VirtualTest](../src/VirtualTest.cpp). Meny-/clipboard-/launch-flyter kontrolleres
  i App; ingen generell angre-, batchnavn- eller egenskapsredigeringshandling er registrert.
- A01-A06/R01-R09: [ArchiveFile](../src/ArchiveFile.cpp), [NetworkFile](../src/NetworkFile.cpp),
  [VirtualFile](../src/VirtualFile.cpp), [ConnectionView](../src/ConnectionView.cpp)
  og VirtualTest. Bare tilgjengelig dynamisk bibliotek teller ikke som kvalifisering.
- R10-R17/X01-X10: DirectoryScanner, App og VirtualFile viser eksisterende disk-,
  launcher- og providergrense. Ingen av de manglende VCS-/sky-/enhetsproviderne
  er registrert der. [DirectoryScanner](../src/DirectoryScanner.cpp) lister
  monterte volumer; det er ikke det samme som å kunne montere dem fra Listi.

## Navigasjon og arbeidssteder

| ID | Brukerbehov og konkret prøve | Referanse | Listi, basis 0.4; oppdateringer merket | Vår retning |
|---|---|---|---|---|
| N01 | Åpne to ulike steder, velg kilde, kopier uten å miste målet | D1 split-view | V: to paneler, låst jobb og PNG-lesekontroll, UI | Behold tydelige kilde/mål-roller og uavhengige paneler |
| N02 | Skriv en sti; gå til forelder, hjem og rot | D1 location-bar; D2 | D: redigerbar sti og Go-meny; forelder prøvd, ikke alle snarveier på 0.4 | Native stifelt og lokal navigasjon, uten ny global kommandogrammatikk |
| N03 | Besøk A, B, C; gå tilbake til B og frem til C i bare ett panel | D1 dolphin-view-using; D2 | V (0.5): uavhengig panelhistorikk, også nettverk og arkiv | NAVIGATION-0.5.0.md. Bevar kilde/mål og tekstfeltgrenser |
| N04 | Hopp direkte til en vilkårlig forfar uten å skrive hele stien | D1 location-bar-bread-crumb | D: forelder og tre finnes; ingen direkte valg av alle stileddene | Lag vår egen raskt tilgjengelige stinavigasjon |
| N05 | Lagre en mappe/deling som favoritt, åpne etter omstart | D1 places-panel, quick-bookmarking | V (0.6): navngitte steder med omstart og redigering | PLACES-0.6.0.md. Innloggingsdata forblir utenfor stedslagringen |
| N06 | Hold flere arbeidssteder åpne og bytt uten å navigere på nytt | D2 new_tab; D1 File-meny | M: to paneler, ikke faner/flere arbeidssett | Flere Listi-arbeidssteder med tydelig aktivt kilde/mål-par |
| N07 | Åpne et separat filarbeidsvindu og lukk det uten å avslutte alt | D2 new_window, popout_split_view | M: ett hovedvindu | Native vinduslivsløp, køeierskap og lukk/avslutt-skille |
| N08 | Start igjen med valgte steder og arbeidsoppsett bevart | D1 preferences-dialog-startup; D2 sesjonslagring | D: visningsvalg lagres; steder og arbeidsøkten gjenopprettes ikke | Eksplisitt gjenopptakelse, ikke automatisk restart av filjobber |
| N09 | Klikk eller bruk tastatur i hvert mappetre uten å endre motsatt panel | D1 folders-panel | V: begge trær, Tab og nye mapper, UI | Behold uavhengige trær og avgrenset mellomlager |
| N10 | Åpne et sted fra et annet program eller kommandolinjen | D1 preferences-dialog-startup; D2 | D: scripting kan sette panelsti; generell refs/URL/argv-åpning mangler | Native refs og eksplisitt plassering av nye forespørsler |

## Finne, vise og velge

| ID | Brukerbehov og konkret prøve | Referanse | Listi, basis 0.4; oppdateringer merket | Vår retning |
|---|---|---|---|---|
| V01 | Bytt mellom detaljer, ikoner og visuell bildeliste | D1 dolphin-view-modes; D3 | V: detaljer, ikoner/store ikoner og thumbnails prøvd, UI | Native visninger styrt av én panelmodell |
| V02 | Se svært mange navn tett, og juster visningsskala uten å miste valg | D1 Compact; D3 zoom | D: faste ikonstørrelser; ingen tett kolonnevisning eller fri zoom | Utform plassøkonomisk Listi-visning, ikke kopier referansens rutenett |
| V03 | Sorter navn/størrelse/tid naturlig og velg retning i alle visninger | D3 Sort By | D: detaljkolonner sorterer; modell/grid har egen navneorden; ingen samlet naturlig sortering | Sorteringsregel i modellen med samme resultat i alle visninger |
| V04 | Gruppér filer etter type, tid eller annen egenskap | D3 Group By | M | Modellbaserte grupper som ikke endrer filoperasjonenes utvalg |
| V05 | Vis/skjul punktfiler, uten å forveksle dette med navnefilter | D3 Show Hidden Files | D: punktfiler vises; egen vis/skjul-bryter mangler | Eksplisitt panelvalg med presise utvalgsregler |
| V06 | Velg et intervall, krymp det, inverter eller velg alle synlige filer | D1 selection; D2 | V: modelltester og grid-intervall/drag-bevis, T/UI | Behold ett sannhetsgrunnlag for valg og fokus |
| V07 | Snevre inn navn mens du skriver; nullstill uten skjult filvalg | D1 filter-files | D: bokstavelig filter er prøvd; ikke samme mønster-/regexmuligheter | Raskt lokalt filter først; eventuelle mønstre må være eksplisitte |
| V08 | Finn et filnavn i undermapper, avbryt og se treffets opprinnelse | D1 finding-searching-in-file; K1 filenamesearch | D (0.7): lokalt avbrytbart navnesøk og BFS-snapshot | Se SEARCH-0.7.0.md; virtuelle filkilder gjenstår |
| V09 | Finn innhold, type, dato og metadata i et større filområde | D1 søk; Dolphin Baloo-avhengighet | D (0.8): native indeksformler og levende attributtprøver; innhold/vennlig byggeverktøy mangler | Bruk BFS-indekser/queries der de passer, tydelig fallback ellers |
| V10 | Lagre et søk og åpne oppdaterte treff senere | D1 søk, Save | V (0.8): lokale søkevilkår lagres og kjøres på nytt; installasjon/tilbakeføring prøvd | SEARCH-0.8.0.md. Virtuelle filkilder og query-filutveksling er egne restkrav |
| V11 | Se type, eierskap, rettigheter, mediemetadata og valgte kolonner | D1 dolphin-view-information; D3 visible roles | D (0.10): lokal egenskapsflate med typed attributter, eier-ID og rettigheter; medievisning/kolonnevalg gjenstår | Native MIME/BFS-egenskaper og formatspesifikke tillegg |
| V12 | Legg på og søk i merker, rangering og kommentarer | D1 information-panel og søk | M | Egen forståelig metadataflyt; BFS-attributter vurderes før format låses |
| V13 | Se thumbnails for bilder, dokumenter, video og andre installerte typer | D1 preview; K1 thumbnail | D: bildeflyt prøvd med PNG; øvrige translators/formater er ikke bredt kvalifisert | Haiku Translators først; avgrenset og avbrytbar dekoding |
| V14 | Forhåndsvis bilde, tekst, lyd/video eller dokument uten å bytte arbeidssted | D1 information-panel | D: bilde og begrenset tekst; lyd/video/dokumentvisning mangler | Egen preview-flate med formatkapabiliteter og tydelig livsløp |
| V15 | Husk en mappes visning, start på nytt, velg standard igjen | D1 view-properties; D3 | V: mappeoverride, omstart og reset, UI | Behold eksplisitte lokale valg, utvid bare ved behov |
| V16 | Tilpass snarveier, knapper og plassbruk | D1 configuration; D2/D3 | D: font/farger/knapper/kolonner; snarveier og arbeidsoppsett mangler | Gode standardvalg og gradvis tilpasning |
| V17 | Bruk tastatur og stor skrift i lyst/mørkt systemoppsett | D1 configuration; D2 | D: normalfont og 1024x650 prøvd; stor skrift, hjelpemidler og andre keymaps ikke kvalifisert | Native fokus og tilgjengelighet, med egen testmatrise |
| V18 | Utfør relevante filhandlinger der valget er, også uten tastatur | D3 basicActionsMenu; D2 selection mode | D: bank/menyer finnes; rik lokal kontekst-/berøringsflyt mangler | Listi-handlinger nær valget, felles operasjonsmodell |

## Filhandlinger og gjenoppretting

| ID | Brukerbehov og konkret prøve | Referanse | Listi, basis 0.4; oppdateringer merket | Vår retning |
|---|---|---|---|---|
| F01 | Kopier valgte lokale filer/mappehierarki og les tilbake resultatet | D1 Edit-meny; D3 | V: lokal kopi, metadata/lenker og UI-PNG, T/UI | Viderefør verifisert commit og kilde/mål-kontroll |
| F02 | Flytt lokalt; ikke marker ferdig før kilden er ryddet | D1 Edit-meny | D: native backend bestått, UI-flyttbevis historisk, T/H | Kjør ny sluttbrukerprøve før bredere flyttegaranti |
| F03 | Kopier mellom lokal mappe og en nettverksdeling | D1 KIO; K1/K2 | D: SMB/SFTP/FTP-fixtures bestått, ikke ekstern NAS og bred UI/protokollmatrise | Samme kø og tydelige kapabiliteter per kilde/mål |
| F04 | Flytt mellom ulike filkilder og gjenopprett etter delvis feil | D1 KIO og Edit-meny | D: samme-endepunkt rename/move; krysskilde-flytt avvises | Verifisert kopi/commit før kildeopprydding, ikke bare «kopier og slett» |
| F05 | Klipp/kopier filer og lim inn i Listi eller Tracker | D1 Cut/Copy/Paste; D2 | D (0.9): lokal native utklippstavle begge veier prøvd, også avbrudd og nyere klipp | CLIPBOARD-0.9.0.md. Virtuelle filkilder og utvidede utklippstavlevalg gjenstår |
| F06 | Dra et flervalg og velg kopi, flytt eller lenke | D1 dolphin-view-using | D: lokal flervalg-dragkopi prøvd; flytt/lenke og alle virtuelle retninger ikke dekket | Egen enkel drop-beslutning, frosset mål og utvalg |
| F07 | Lag en mappe med et validert navn fra aktivt panel | D3 create_dir | V: installert inline-spørsmål og mappe på disk, UI | Behold liten native inline-flyt |
| F08 | Opprett en ny fil eller fil fra en mal | D3 create_file; D1 Create New | M | Native filtyper og en liten eksplisitt malflyt |
| F09 | Gi én fil nytt navn, med forståelig konflikt | D1 Rename; D3 | D: backend og historisk UI, ikke ny 0.4 UI-prøve, T/H | Behold frosset original og validering |
| F10 | Gi flere filer systematiske navn uten kollisjoner | D1 batch-rename | M | Forhåndsvis gammel/ny mapping før én avgrenset jobb |
| F11 | Lag en kopi ved siden av originalen med et nytt navn | D1 Duplicate Here; D3 | D (0.13): navngitt lokal duplisering; batch gjenstår | Egen navngitt duplikathandling med kollisjonskontroll |
| F12 | Opprett en symbolsk lenke og gå til lenkens mål | D1 drag/link og Show Target; D3 | D (0.13): relative/absolutte lenker opprettes lokalt; vis mål gjenstår | Native lenkeoperasjoner, ikke implisitt dereferering |
| F13 | Legg i lokal papirkurv og gjenopprett uten å overskrive nyere filer | D1 Trash/Undo; K2 trash | D: native Trash/Restore bestått, aktuell UI ikke fullstendig prøvd, T/H | Haiku/Tracker-kompatibel Trash; ikke falsk remote-Trash |
| F14 | Slett permanent først etter eksplisitt valg og bekreftelse | D1 Delete; D3 | D: backend testet; aktuell permanent-slett-UI ikke repetert, T/H | Behold klart skille fra papirkurv |
| F15 | Angre og gjør om støttede handlinger med synlig konsekvens | D2 FileUndoManager undo/redo | M: gjenoppretting fra Trash er ikke generell angre | Begrenset operasjonsjournal med konfliktsjekk, aldri backup-løfte |
| F16 | Se/endre rettigheter og relevante filegenskaper | D1 Properties; D3 | D (0.10): eksplisitt lokal rettighets- og attributtredigering; virtuelle filkilder/eierbytte gjenstår | Native egenskaper med tydelig støtte per filsystem/protokoll |
| F17 | Sammenlign to filer eller mappetrær | D1 compare-files, valgfri Kompare | M | Egen sammenligningsflyt eller native verktøyintegrasjon med valgt par |
| F18 | Se jobbstatus, pause/avbryt og behold kontroll ved navigasjon | D2/KIO jobbmodell | D: kø/avbrudd og feilbaner prøvd; pause-UI er historisk, T/O/H | Viderefør native kø, synlig delvis resultat og eget eierskap |
| F19 | Velg Replace/Skip/Abort for riktig konflikt; bevar original ved avbrudd | D1 filhandlinger; KIO | V: faktisk installert konflikt/avbrudd og feilinjeksjon, UI/O | Tokenidentifisert spørsmål og dokumenterte gjenopprettingsstier |
| F20 | Åpne valgt fil med riktig app eller et eksplisitt redigeringsverktøy | D1 view-using/Properties | D (0.11): lokal MIME-visning og engangsvalg av native program, installert UI; globale tilknytninger, batch og virtuelle filer gjenstår | Haiku MIME og native appvalg; ingen skjult remote-upload |

## Arkiver, protokoller og eksterne filkilder

| ID | Brukerbehov og konkret prøve | Referanse | Listi, basis 0.4; oppdateringer merket | Vår retning |
|---|---|---|---|---|
| A01 | Gå inn i et arkiv, naviger mapper og gå ut igjen | K1 archive; A1 | D: ZIP/tar-familie native testet; full nåværende arkiv-UI mangler, T | Arkiv som eksplisitt skrivebeskyttet sted i panelet |
| A02 | Trekk ut valgte oppføringer med metadata, uten sti-/lenkerømming | K1; A1 ark-extract | D: native uttrekk og angreps-/metadata-fixtures består, T; bred UI ikke kvalifisert | Samme sikre kø og valgt mål som annen kopiering |
| A03 | Lag, oppdater og legg filer til et arkiv | A1 ark-create, ark-editing-files | M | Egen arkivjobb med forhåndsvisning og atomisk erstatning der mulig |
| A04 | Åpne/lage passordbeskyttede og delte arkiver | A1 ark-password-protection, ark-multi-volume | M | Avgrenset formatstøtte og passordlivsløp, ikke lagring i vanlig oppsett |
| A05 | Åpne enkeltkomprimerte filer og flere tilgjengelige arkivformater | K1 filter/archive; A1 | D: gzip/bzip2/xz og testede ZIP/tar består; 7z/rar/cpio/iso m.fl. ikke kvalifisert, T | Oppgi faktisk runtime-støtte og test hvert lovet format |
| A06 | Bruk arkiver på nett eller inni andre arkiver | A1/KIO: konkret kombinasjon må runtime-avklares | M: bare lokal ytterfil; referansens kanttilfeller også uprøvd | Eget materialiserings-/cachelivsløp før støtte loves |
| R01 | SMB: bla, les, skriv, håndter feil og avbryt mot en deling | K1 smb, valgfri Samba-avhengighet | D: syntetisk SMB består, ikke bred NAS-kvalifisering, T | Egen tilkoblingsflyt og samme panel/kø |
| R02 | SFTP: bruk kjent vertsnøkkel, kopier begge veier, avvis feil vert | K1 sftp, libssh-avhengighet | D: fixture/known_hosts består; første-gangs vertsbekreftelse mangler, T | Tydelig vertstillit og sesjon, native credential-lagring vurderes separat |
| R03 | FTP: bla og overfør med tydelig transportinformasjon | K2 ftp | D: fixture består; flere serverlister og UI-feilbaner gjenstår, T | Bevar synlig informasjon om ukryptert forbindelse |
| R04 | Bruk FTPS med korrekt sertifikatkontroll | Referansens FTPS-dekning ikke fastslått i denne inventeringen | U: Listi/libcurl-adapter finnes, ingen TLS-fixturetest | Eget Listi-krav fra nettverkssporet; ikke tell som bevist Dolphin-gap |
| R05 | WebDAV/HTTPS: bla, kopier og håndter autentisering | D1 KIO; K2 http/WebDAV | M | Protokolladapter i vår eksisterende filmodell |
| R06 | NFS: naviger eksport og overfør med riktig rettighetsmodell | K1 nfs, plattformavhengig | M: bare OS-montert sti kan fungere som lokal mappe, ikke integrert kvalifisert NFS | Native mount-/provider-integrasjon med faktisk feiltest |
| R07 | SSH-filtilgang uten SFTP-server via fish-tilbudet | D1 KIO; K1 fish | M: SFTP erstatter ikke automatisk dette behovet | Vurder egen trygg SSH-transport; behold behovet som udekket |
| R08 | Finn tilgjengelige delinger og gjenbruk lagrede tilkoblinger | D1 Places/KIO; K1 smb/DNSSD | M: manuell adresse og kun sesjonslegitimasjon | Favoritter, kapabiliteter og oppdagelse uten passord i URL |
| R09 | Rediger fjernfil med lokal app og lagre tilbake med konfliktkontroll | KIO-arbeidsflyt; E1 GDrive eksplisitt | M: Listi krever lokal kopi, ingen automatisk retur | Egen arbeidskopi med versjonssjekk og synlig publiseringssteg |
| R10 | Bruk minnepinne/disk og monter/løs ut fra filarbeidet | D1 mounting-storage-media | D: Disks og monterte stier vises; montering/utløsing ikke integrert eller kvalifisert | Native volum- og mount-API, behold Tracker-sameksistens |
| R11 | Overfør til/fra MTP-enhet | K1 mtp, valgfri libmtp | M | Egen enhetsprovider, prøvd fysisk før støtte loves |
| R12 | Overfør til/fra Apple-enhet via AFC | K1 afc, valgfri libimobiledevice | M | Native enhetsintegrasjon, separat fysisk kvalifisering |
| R13 | Hent bilder fra PTP-kamera | E1 Kamera | M | Egen importflyt med duplikatkontroll og originalbevaring |
| R14 | Bla og overfør i Google Drive | E1 KIO GDrive 26.08.3 | M | Google Drive-provider med egne kontogrenser og OAuth-flyt |
| R15 | Bla, les og skriv objekter i S3 | E1 KIO S3 1.0.2 | M | S3-provider som viser objektmodellens faktiske begrensninger |
| R16 | Se Dropbox-status og relevante synkroniseringshandlinger | P1 dropbox | M | Egen tjenesteintegrasjon; vanlig lokal mappe er ikke status-/sync-dekning |
| R17 | Bruk Nextcloud-status, synkronisering og deling | E1 appside; ekstern integrasjon, versjon må festes | M | Skill WebDAV-filtilgang fra synkronisering og delingsfunksjoner |

## Integrasjoner og bredde

| ID | Brukerbehov og konkret prøve | Referanse | Listi, basis 0.4; oppdateringer merket | Vår retning |
|---|---|---|---|---|
| X01 | Åpne terminal i valgt mappe og hold shell-/filkontekst forståelig | D1 terminal-panel | D: ekstern Terminal og kommandofelt; ingen integrert toveis mappefølge, H | Native terminalintegrasjon med tydelig valgt mappe og prosesseierskap |
| X02 | Se versjonsstatus og utfør relevante repository-handlinger | P1 git/svn/hg/bazaar | M | Små provider-adaptere; hver støttet VCS får egen akseptprøve |
| X03 | Monter/løs ut en diskavbildning og se status | P1 mountiso/mountedisooverlay | M | Native avbildnings-/volumstøtte, ikke KDE-modulimport |
| X04 | Samle filer fra flere steder uten å flytte originalene | E1 FileStash | M | Virtuell samling med synlig originalsti og presis slettesemantikk |
| X05 | Hent/konverter spor fra lyd-CD | E1 KIO AudioCD | M | Native medie-/importflyt, separat fysisk kvalifisering |
| X06 | Tilby relevante delings- og egne konteksthandlinger | D1 Services; E1 appside | M: fast handlingssett, ikke et utvidbart tjenestetilbud | Deklarerte handlinger som bruker vårt frosne utvalg og mål |
| X07 | Gjør autorisert filarbeid som krever høyere rettigheter | AD, separat administrativ worker | M: ingen tilsvarende delegert rettighetsflyt | Plattformtilpasset, eksplisitt handling; ikke kjør hele UI som administrator |
| X08 | Finn nylige filer/aktiviteter og andre virtuelle arbeidssteder | K1 recentlyused/activities; D1 Places | M | Egne nylige steder/arbeidsflater med forståelig historikk og personvern |
| X09 | Åpne program-, font-, hjelp- og systemsteder som del av filarbeidet | D1 KIO; K1 info/man; K2 help | D: vanlige lokale systemmapper kan åpnes; spesialiserte virtuelle steder mangler | Haikus egne systembegreper og apper, ingen KDE-navneromskopi |
| X10 | Se fri plass og størrelsen på valgt mappe/utvalg | D1 statusbar/Properties | D: filstørrelser og antall vises; fri plass/rekursiv størrelsesflyt mangler | Avbrytbar bakgrunnsberegning og native voluminfo |

## Opprinnelig prioritering fra basiskartet

Punkt 1 og 2 er levert i 0.5/0.6, og første søkeflyt i 0.7/0.8.
[Nåværende kø og nye designkrav](ROADMAP.md) er autoritativ for videre arbeid.

1. **Naviger tilbake dit du var.** N03, med N02/N09 som regresjoner. Uavhengig
   Tilbake/Frem i hvert panel, tilgjengelig for mus og tastatur, som også tåler
   arkiv/nettverksadresser. Ingen ny pakkeprofil, lagret hemmelighet eller generell
   angre-motor. Detaljert ferdigkrav står i [neste trinn](NEXT-NAVIGATION.md).
2. **Gjenfinn viktige arbeidssteder.** N05/R08: favoritter med ufarlige adresser,
   omstart og redigering/sletting. Legg ikke passord i favoritter. Verts-/konto-
   lagring må få et konkret native design, ikke gjettes inn i en oppsettsfil.
3. **Finn en fil i en mappegren.** V08: avbrytbart navnesøk med originalsti og
   eksplisitt rot. Deretter V09/V10 med BFS-støtte. Filter skal fortsatt være lett.
4. **Kvalifiser de filkildene vi allerede viser.** R01-R04/A01-A06: faktiske
   protokoll-/UI-scenarier, FTPS-fixture og tydelig formatliste før nye brede løfter.
   Egen NAS/hardware krever tilgang; syntetiske prøver kan gjøres uavhengig.
5. **Gjør filarbeid på tvers av programmer.** F05/F06/F16: Tracker-utklippstavle,
   drop-semantikk og egenskaper før flere kraftige massehandlinger.
6. **Trygg massebehandling og gjenoppretting.** F10/F15/F17: forhåndsvisning,
   kollisjoner og konfliktbevisst journal/sammenligning. Ikke universell angre.
7. **Utvid kilder og integrasjoner i små vertikale løp.** R05-R17/X01-X10. Hvert
   tjenestenavn og hver protokoll er en egen kvalifisering, ikke én «sky»-avhuking.

Dette prioriterer arbeid, ikke bortvalg av restkrav. Vi publiserer ikke offentlig
før den avtalte funksjons- og ytelsesporten faktisk er dokumentert oppfylt.

## Ytelsesport: fremtidig måling, ingen oppfunne sammenligningstall

Bruk samme fysiske maskinvare der operativsystemene tillater det, eller merk
resultatene som ikke direkte sammenlignbare. Registrer OS, app- og bibliotek-
versjoner, CPU/RAM, filsystem, disk, nettverk/server og skjerm/font. Skill kald og
varm cache. Dolphin-tillegg/Baloo-indeksering og Listi-preview må være eksplisitte.

| Datasett/scenario | Begge kandidater må måles på |
|---|---|
| 1 000 / 10 000 / 100 000 lokale navn med lange og norske tegn | Oppstart, første brukbare liste, full liste, sortering/filter, UI-svar, CPU, minne og disk-I/O |
| 1 000 blandede bilder med små/store og defekte filer | Første thumbnails, peak/steady minne, scrolling, avbrudd og avslutning |
| 10 000 filer over kontrollert SMB/SFTP med fast nettverkslatens | Første liste, gjennomstrømning, UI-respons, avbrudd og gjenoppretting |
| ZIP/tar og enkeltkomprimerte filer av faste størrelser | Liste/uttrekkstid, bytes, metadata, minne og avbrudd |
| 60 sekunder tomgang samt 30 minutters gjentatt navigasjon | CPU, resident/privat minne, handles/tråder og respons før/etter |

De eksisterende ti-sekunders Listi-prøvene er bare en lokal observasjon, ikke
Dolphin-sammenligning eller stabilitetsbevis. Ingen dekningsprosent beregnes fra
radantall: en enkel menyhandling og en hel protokoll er ikke like store enheter.

## Gjenstående arbeid i selve referansekartet

Kjør den fastsatte Dolphin-kombinasjonen i et isolert referansemiljø før vi bruker
matrisen som likhetsbevis. Fest versjoner og faktiske handlingssett for de øvrige
annonserte tilleggene. Avklar FTPS, fjern-/nestede arkiver, provider-avhengig
resume, autentisering, batch-/undo-grenser og tilleggenes egne feilstier. Legg
nye behov inn med stabil ID; ikke skriv dem om til «dekket» fordi en nærliggende
Listi-funksjon eksisterer. Kartet er nå et etterprøvbart første arbeidsgrunnlag,
ikke en uttømmende sertifisering av hele tredjepartsøkosystemet.

[apps]: https://apps.kde.org/dolphin/
[handbook]: https://github.com/KDE/dolphin/blob/2fdee206603fd3a21928984e9a3d404e82ae9890/doc/index.docbook
[actions]: https://github.com/KDE/dolphin/blob/2fdee206603fd3a21928984e9a3d404e82ae9890/src/dolphinmainwindow.cpp
[viewactions]: https://github.com/KDE/dolphin/blob/2fdee206603fd3a21928984e9a3d404e82ae9890/src/views/dolphinviewactionhandler.cpp
[extras]: https://github.com/KDE/kio-extras/blob/v26.08.3/CMakeLists.txt
[archivejson]: https://github.com/KDE/kio-extras/blob/v26.08.3/archive/archive.json
[workers]: https://github.com/KDE/kio/blob/v6.23.0/src/kioworkers/CMakeLists.txt
[ark]: https://github.com/KDE/ark/blob/v26.08.3/doc/index.docbook
[plugins]: https://github.com/KDE/dolphin-plugins/blob/v26.08.3/CMakeLists.txt
[gdrive]: https://apps.kde.org/kio_gdrive/
[s3]: https://apps.kde.org/kio_s3/
[admin]: https://github.com/KDE/dolphin/blob/2fdee206603fd3a21928984e9a3d404e82ae9890/src/admin/workerintegration.cpp
