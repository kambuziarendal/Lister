# Arbeidsplan for Listi

**Tilleggskrav 14. november 2026:** Alle Trackers brukeroppgaver skal også
dekkes i Listi, på vår egen måte. Se [Tracker-kartet](TRACKER-COVERAGE.md)
for dokumentert tilbud, udekkede behov og akseptkrav. Dette erstatter ikke
Dolphin-kravet og er ikke en påstand om ferdig funksjonslikhet.


**Tilleggskrav 15. november 2026:** Arkiver skal så langt formatet tillater det
oppføre seg som mapper i filbehandleren, med samme filhandlinger og uten separate
arbeidsflyter. Dette gjelder alle støttede arkivformater, ikke bare ZIP og 7z.
0.25.0 utvider mappehandlingene. 0.26.0 legger til klipp/lim inn og flytting
mellom skrivbare lokale arkiver med verifisering før kildeopprydding.
0.27.0 legger til ekstern redigering med verifisert lagring tilbake og private gjenopprettingskopier.
Full likhet, flytting mellom lokale mapper og arkiver,
lenker/spesialmetadata, flerpartsarkiver og skrivemotorer for RAR gjenstår. Ikke merk kravet ferdig før disse grensene er løst.
**Nytt krav 15. november:** Rekursiv mappe-diff med størrelseskontroll og valgfri SHA-256.
Kontekstmenyen skal kunne velge alle filer av samme type som den klikkede filen.
Implementert i lokal kandidat 0.28.0: skrivbeskyttet rekursiv sammenligning med
størrelse/SHA-256 og valg etter filendelse i det aktive panelet.

Skipta er nytt arbeidsnavn for den planlagte delen tidligere omtalt som Deila.

14. november 2026. Første mål er en god, særpreget filbehandler til daglig bruk.
Vi leverer små, ferdige arbeidsflyter som samtidig peker mot den større visjonen.
Vi bygger videre på 0.2.0. Ingen stor omskriving er en forutsetning.

## Åpen kildekode, besluttet 14. november 2026

Arne har valgt åpen kildekode som fast retning for Listi, i tråd med
Haiku-økosystemet. Lukket kildekode er ikke lenger et produktmål.
Eksisterende MIT-lisens for vår kildekode endres ikke ved denne beslutningen.
Lisensvilkår, kildekodetilgjengelighet og nødvendige merknader for avhengigheter
og den samlede distribusjonen skal følges, inkludert Samba/SMB.
Åpen kildekode utelukker ikke senere betalte pakker, støtte eller donasjoner;
ingen betalingsmodell er besluttet. Beslutningen gir ikke i seg selv fullmakt
til offentlig publisering. Publiseringsgrensen nedenfor gjelder fortsatt.

## Arbeidsform og publiseringsgrense, oppdatert 14. november

Det haster ikke. Nye ønsker legges i kø, ikke automatisk inn i pågående kodearbeid.
Vi fullfører én avgrenset, gjennomgått og testet arbeidsflyt om gangen. En samlet
intern kandidat kan inneholde flere slike trinn; den skal ikke bygges som én stor,
uverifisert endring. Rene modulgrenser og forståelig kode er ferdigkrav.

**Listi holdes intern.** Ingen nye offentlige utgivelser, offentlige kandidatfiler
eller publisering av kandidatgrenen. Tidligere offentlig Lister-historikk endres
ikke uten egen avklaring. Intern betatesting og privat pakkeleveranse kan fortsette.

Målet før offentlig lansering er minst Dolphins funksjonsdekning, med bedre
respons og vesentlig lavere ressursbruk. Dette er et mål, ikke en dokumentert
egenskap ved kandidaten. Før lansering må vi lage en sporbar funksjonsmatrise mot
en navngitt Dolphin-versjon, inkludert relevante tillegg og protokoller. Hvert
punkt trenger faktisk brukerflyt og kjent teststatus. Native Haiku-løsninger kan
oppfylle samme behov uten å kopiere KDEs implementasjon.

Ytelse må måles med dokumentert maskinvare, datasett og varm/kald cache:
oppstart, store mapper, søk/filtrering, thumbnails, nettverk og filoperasjoner.
Registrer ventetid, CPU, minne og disk-I/O, også tomgang og avbrudd. Sammenligning
på forskjellige operativsystemer må beskrive forskjellene. Vi lover ingen bestemt
ressursbrøk før målingene finnes. Tracker-erstatning er en senere, separat etappe.

### Intern kandidat 0.4.0, kontrollert 14. november

Sikkerhet/modulgrenser, panelvisninger og intern operasjonsvisning er samlet i en
privat kandidat. Native tester, kompakt lys/mørk visning, faktisk installasjon og
0.3.0-tilbakeføring/ny oppgradering er kontrollert. Se ACCEPTANCE-0.4.0.md.
Neste produkttrinn er funksjonsmatrisen og et konkret betabehov, ikke automatisk
publisering eller en ny stor omskriving. De opprinnelige deltrinnene følger under
som historikk og retning; dette er ikke en påstand om full Dolphin-dekning.

### Nåværende trinn og kø, oppdatert for 0.13.0

1. Sikkerhet/modulgrenser, arkiv/nettverksgrunnlag, panelvisninger og intern
   operasjonsvisning er samlet og kvalifisert innenfor den private 0.4.0-kandidatens
   uttrykkelige begrensninger. Dette betyr ikke full protokoll- eller funksjonsdekning.
2. Første [funksjonskart](FUNCTION-COVERAGE.md) er laget mot Dolphin 26.08.3,
   med separate protokoller/tillegg, brukerflyter og teststatus. Referansen er
   kildekontrollert, ikke runtime-kvalifisert. Den er ikke en designoppskrift.
3. **Uavhengig navigasjonshistorikk er implementert i privat 0.5.0.**
   Atferd og verifikasjon står i [NAVIGATION-0.5.0.md](NAVIGATION-0.5.0.md).
   Opprinnelige ferdigkrav beholdes i NEXT-NAVIGATION.md.
4. Lagrede arbeidssteder og målrettet bughunt inngår i privat 0.6.0. Se
   [atferd og verifikasjon](PLACES-0.6.0.md).
5. Avbrytbart lokalt navnesøk og native BFS-formler er implementert som
   snapshot-arbeidsflate i privat 0.7.0. Se [søk og begrensninger](SEARCH-0.7.0.md).
6. **Levert i privat 0.8.0:** levende BFS-treff og lagrede søkevilkår. Nye/fjernede
   treff, endret identitet, avbrudd, robust lagring og 0.7.0-tilbakeføring er prøvd.
   Se [akseptkvitteringen](ACCEPTANCE-0.8.0.md). Attributtbygger, innholdssøk,
   query-filutveksling og flere volumer beholdes som egne søketrinn.
7. **Neste dagligflyt:** Lokal Tracker-utklippstavle levert i privat 0.9.0,
   med tastatur/mus, identitetskontroll og avbrudd. Egenskaper og typed lokale
   BFS-attributter inngår i 0.10.0, se PROPERTIES-0.10.0.md for avgrensning.
   MIME-visning og engangsvalg av program er kvalifisert i privat 0.11.0, se
   OPEN-WITH-0.11.0.md. Standardtilknytninger og MIME-redigering gjenstår.
   Tydelig lokal drop-semantikk inngår i privat 0.12.0, se DROP-0.12.0.md.
   Lokale lenker og duplisering inngår i privat 0.13.0, se NAMED-ENTRIES-0.13.0.md.
   Neste avgrensede flyt er arkivoppretting med tydelige formatvalg. Dypere kvalifisering av eksisterende
   protokoller/arkivformater følger relevante endringer, inkludert egen FTPS-prøve.
8. **Trygg massebehandling:** maler, lenker, duplisering, masseomdøping med
   forhåndsvisning, sammenligning og konfliktbevisst angre-journal.
9. **Arbeidsflater:** utforsk native mappekort og arbeidssett som beskrevet under.
   Flere vinduer og øktgjenoppretting skal bevare filjobbenes opprinnelige mål.
10. **Flere filkilder:** egne adaptere og ende-til-ende-prøver for protokoller,
    enheter, sky og versjonskontroll. Fysisk utstyr må prøves før kvalifisering.

### Mappekort, designretning fra samtalen 14. november

Arne ønsker en pen BeOS-aktig løsning for tabs, ikke vanlige nettleserfaner.
Utforsk kompakte, navn-tilpassede skillekort over hvert panel. Aktivt kort skal
henge visuelt sammen med filflaten, med native ikon, fine kanter og varsomt
relieff. En varm aktivmarkering må fungere med begge systemtemaer og ikke være
eneste signal. Unngå permanente lukkekryss på alle kort og uleselig krymping.
Ved mange kort skal en liste over åpne steder bevare lesbare navn.

Hvert kort skal ha egen mappe, historikk, visning og utvalg. Dra for å ordne eller
flytte mellom panelene må ikke endre pågående filjobber. Arbeidssett er et eget
begrep som lagrer begge panelene samlet, ikke bare et annet navn på en fane.
Lag og prøv et par visuelle forslag i faktisk Haiku-størrelse før implementasjon:
mange mapper, lange navn, kompakt vindu, mus/tastatur og lys/mørk visning.
Dette er en designretning, ikke en ferdig løsning eller en endring av pågående søketrinn.

### Arkivoppretting og korte kontekstmenyer, krav fra 14. november

- Opprett arkiv av ett eller flere markerte elementer via både tastatursnarvei
  og musehandling. Samme kommando, frosset kildeutvalg og samme innstillinger.
- Velg favorittformater for komprimering, standardformat og formatspesifikke
  standardvalg. Bare faktisk tilgjengelige skriveformater kan tilbys som støttet.
- Egen innstilling «spør om format hver gang». Vis favoritter først og resten
  under «Flere formater». Lagret standard skal ikke omgå et aktivt spør-valg.
- Tastaturflyt og museflyt skal begge dekke valg av format, navn/mål, innstillinger,
  konflikt, fremdrift og avbrudd. Sikker staging/commit skal bevare originalfiler
  og eksisterende målarkiv ved feil. Pakken må kunne pakkes ut og verifiseres.
- Hold høyreklikkmenyen kort og relevant for fil, mappe, utvalg og provider.
  Grupper eksempelvis Pakk, Åpne med og Flere filhandlinger i grunne undermenyer.
  Ikke legg alle fremtidige funksjoner i en lang toppnivåliste.
- Begynn med én forutsigbar klikkemåte. Eventuelle alternative klikkemoduser må
  være et eksplisitt, synlig valg med full tastaturtilgang, ikke skjult modusbytte.
  Test menyoversikt med stor funksjonsbredde og mange installerte apper.

Dette er akseptkrav for kommende arkiv-/menytrinn, ikke funksjoner i 0.7.0.

Arnes presisering: Vi skal ikke kopiere Dolphin på noen måte. Det de tilbyr av
funksjoner skal vi også dekke, gjennom vår egen kode, utforming og arbeidsflyt.
Native Haiku-løsninger skal brukes der de løser behovet godt. Felles funksjonsnavn
betyr ikke at implementasjon, plassering, utseende eller tastaturkart skal kopieres.

Tilpasning følger DESIGN.md: gode standardvalg, lokale panelkontroller,
eksplisitte mappevalg og en enkel vei tilbake. Lagrede arbeidsoppsett som «Laptop»
og «Stor skjerm» ligger fortsatt i kø. Kartlegging og nye prioriteringer gir
ikke automatisk fullmakt til offentlig publisering eller nye kostnader.

Fast designkrav: bruk native Haiku-ikoner og brukerens systemoppsett der Haiku
støtter det. Fil-, mappe- og programikoner hentes fra systemet, ikke fra et eget
Listi-tema. Ikonene i konseptbildet er plassholdere, ikke en grafisk spesifikasjon.
Snarveiene skal ligge nær fingrene: kommandotast pluss bokstav som hovedspor,
ikke F-raden. Følg Haikus tastaturoppsett og bevar tekstfeltets vanlige snarveier.
F-tastene kan beholdes som sekundære valg. Gjør bindingene konfigurerbare senere.

## Privat kandidat 0.3.0, 14. november

Listi-navn, kompatibel oppgradering fra Lister, synlige bokstavsnarveier og trygge
tekstfeltsnarveier er implementert. Native filter/kopier/flytt/navn/ny mappe/Trash/
gjenopprett er prøvd, inkludert avbrudd, konflikter og navigasjon mens jobb venter.
Innstillingene beholdes. Verifikasjon og begrensninger står i VERIFICATION.md.
Dette er en privat kandidat, ikke en offentlig utgivelse eller fysisk maskintest.

## Første milepæl: finn, velg og handle med oversikt

1. Rask innsnevring i hvert panel. Synlig navnefilter, tastatursnarvei, tydelig
   resultatantall og enkel nullstilling. Bare synlige, valgte filer følger med
   i operasjoner. Foreldremappen er fortsatt tilgjengelig. Dette bygges først.
2. Gjennomgå flyten med konkrete oppgaver: finn en fil, rydd en mappe, kopier et
   utvalg, håndter en kollisjon, avbryt og gjenopprett fra papirkurven. Rett de
   faktiske friksjonspunktene før vi legger på flere funksjoner.
3. Gjør kilde, mål og operasjonsresultat tydelig gjennom hele oppgaven. Jobber
   beholder sitt valgte mål og utvalg selv om vi bytter fokus eller navigerer.
   Skill fullført, delvis fullført, avbrutt og feilet. Ingen universell angre-lovnad.

Ferdigkrav per leveranse: native bygg, relevante automatiske tester og faktisk
brukerflyt i Haiku. Prøv normal/kompakt størrelse og lys/mørk systemvisning.
Bevar innstillinger og sikre filoperasjoner. Skill testet kandidat fra publisering.

## Neste små leveranser: faktisk daglig filarbeid

Prioritet endret etter Arnes bruk av 0.3.0, 14. november:
Lever SMB, SFTP, FTP, arkivnavigasjon og vindussnarveier samlet i én versjon.
Arkiver som mapper er et umiddelbart kjernekrav, og bygges tidlig sammen med
nettverksgrunnlaget. Ingen separat offentlig SMB-delleveranse. Egne Source- og
Destination-knapper fjernes: aktivt panel er kilde, motsatt panel er mål.
Mål og utvalg fryses ved jobbstart og endres ikke når fokus eller navigasjon endres.

1. **SMB/Samba først.** Åpne en nettverksdeling i et panel, naviger og kopier
   filer begge veier mot det lokale panelet. Tydelig tilkoblingsstatus,
   forståelige innloggingsfeil, fremdrift og avbrudd. Ikke lagre passord i
   URL-er, logger eller vanlig konfigurasjon. Test mot en isolert SMB-server
   før Arnes egen deling. Dette skal gjøre reell betatesting mulig.
2. **Arkiver som mapper.** Enter/dobbeltklikk navigerer inne i ZIP og andre
   tilgjengelige formater, ikke automatisk over til Expander. Oppdag faktisk
   installert backend-/formatstøtte, ikke bare MIME-tilknytning. Naviger,
   velg og trekk ut til det andre panelet; Expander er et eksplisitt alternativ.
   Første steg er lesing/uttrekk, ikke redigering inne i arkivet. Begrens
   uttrekk til valgt mål, også ved absolutte stier, `..` og lenker i arkivet.
3. **SFTP og FTP.** Samme panel- og jobbopplevelse som SMB. Bruk systemets
   tilgjengelige protokollstøtte, valider SSH-vertsnøkler og TLS der relevant.
4. **Tastatur uten overraskelser.** Skill lukk aktivt hjelpevindu fra avslutt
   Listi; kommandotast+W, Escape for dialoger, kommandotast+Q for appen.
   Avklar hovedvinduets livssyklus, bevar jobber og vis alltid aktivt fokus.

Forhåndsvisning, thumbnails, mappevalg og mappetrær inngår i den interne køen
ovenfor. Kommandopalett, batch-navneendring og større arbeidsoppsett prioriteres
etter sikre grunnoperasjoner. Ingen kosmetisk milepæl skal fortrenge filarbeidet
Arne trenger i hverdagen.

## Senere visjoner som første etappe skal tåle

- BFS-attributter og levende søk som arbeidsflater. Vis originalens plassering;
  sletting av treff er sletting av originalen. Lås filutvalget ved jobbstart.
- Sammenligning/synkronisering med forhåndsvisning følger etter brukbare
  arkiv- og nettverkspaneler.
- Portable deler trekkes ut når en konkret funksjon har nytte av det. Ikke bygg
  et stort plattformrammeverk på forskudd. Ny grammatikk, konfigurasjonsmigrasjon
  og varige plattformkontrakter skal avklares før de låses.
- Tracker-erstatning krever egen studie av BFilePanel/libtracker, skrivebord,
  volumer, MIME, åpne-med, Trash, Deskbar og recovery. Sameksistens er standard.
- Plan 9 holdes mulig gjennom separerbare begreper og adaptere. Ingen Plan 9-kode nå.
- [Deila](DEILA-SPEC.md) er et separat, langsiktig prosjekt for kryptert
  peer-to-peer-deling. Listi kan få en valgfri `deila://`-adapter først etter at
  Deila fungerer selvstendig. Deila er ikke en Listi-avhengighet, og planen gir
  ingen fullmakt til implementasjon, nye avhengigheter eller kryptografivalg nå.

## Retningsprøven

Hvert steg skal gjøre en konkret oppgave bedre i dag, passe naturlig på Haiku og
gi plass til videre utvikling. DOpus gir handlefriheten, moderne arbeidsmåter
former flyten. Vi trenger ikke alt samtidig for å ha en tydelig egen identitet.

### Tastatur og innmeldt layoutfeil, 14. november

Arne vil ikke bruke funksjonstastene. Fra 0.13.0 fjernes alle Listi-bindinger
på F-taster og deres knappetekster. Alt+W beholdes som Haikus vanlige
vinduslukking. Inline spørsmål skal ikke endre hovedvinduets størrelse,
inkludert to detaljpaneler uten mappe-/forhåndsvisning.

### Arbeidsoppsett, godkjent plan 14. november

Arne godkjente retningen Enkel, Utforsker, Commander og Arbeidsbenk, men vil
at vi tar det gradvis uten hast. Planlagt, ikke implementert i 0.14.0.

- Enkel: ett filpanel, ingen nederste handlingsknapper. Tracker-aktig flyt.
- Utforsker: mappetre og ett filpanel, ingen nederste handlingsknapper.
- Commander: to filpaneler og dagens handlingsknapper.
- Arbeidsbenk: to paneler, mappetre, forhåndsvisning og flere tilgjengelige
  verktøy. Søk og operasjonslogg åpnes ved behov.

Valg under Vis > Arbeidsoppsett. Samme filhandlinger og snarveier i alle,
separat ikoner/detaljer per panel. Bytte skal bevare mapper og pågående jobber;
siste valg huskes og oppsett skal kunne tilpasses. Ett avgrenset, kvalifisert
trinn om gangen; ikke fire samtidige omskrivinger. Fullfør nåværende rettinger
og høyreklikk før oppsettarbeidet. Konkret rekkefølge og detaljutforming kan
justeres når hvert trinn planlegges.

### Neste avgrensede trinn: 0.15.0

Enkel og veksling tilbake til Commander er implementert først. Utforsker og
Arbeidsbenk står fortsatt i planen. Privat kvalifisering dokumenteres separat.
Se WORKSPACES-0.15.0.md for konkret oppførsel.

### Arkiver, presisert av Arne 15. november

Bred formatdekning skal betjenes direkte i Listi. Det er ikke tilstrekkelig å
åpne Expander eller et annet arkivprogram. Bruk systemets biblioteker og verktøy
som bakgrunnsmotorer, med utskiftbare arkivadaptere og valgfrie ekstra pakker via
HaikuDepot. Ikke bygg alle motorer inn i appen eller lag et eget pakkesystem.
Oppdag installert støtte ved bruk; vis konkret manglende motor ved ukjent format.
Kvalifiser lesing, browsing og uttrekk per format, inkludert 7z/RAR og relevante
krypterte/oppdelte/feilaktige prøver. Ikke lov alle varianter før de er prøvd.
Simple-forenklingen fullføres først. Arkivarbeidet følger som eget avgrenset trinn.

0.16.0 implements the first optional system-libarchive adapter and qualifies a
broader read/extract matrix, with lz4/zstd_bin helpers installed separately.
Follow-up: password UI, multipart inputs, additional engines for unsupported
families and archive extraction from the single-pane workspace. Do not substitute
an external archive GUI for those missing internal workflows.

### Utforsker og kodegjennomgang, privat 0.17.0

Utforsker er implementert som neste trinn: mappetre og ett filpanel, uten
nederste handlingsknapper. Enkel og Commander beholdes. Se
[arbeidsoppsett](WORKSPACES-0.17.0.md) og [kodeaudit](AUDIT-0.17.0.md).
Arbeidsbenk, mappekort/arbeidssett og gjenstående arkivfunksjoner ligger fortsatt
i køen. Kodeaudit er ikke bevis på feilfrihet eller full ekstern formatdekning.

### Arkivtrinn 0.18.0

Ny privat kandidat: oppretting med favorittformat, standard og komprimeringsnivå,
og utpakking til en ny lokal mappe i alle tre oppsett. Opprinnelige arkiver
og eksisterende mål blir aldri erstattet. Se ARCHIVES-0.18.0.md for grenser.
Passord, delte arkiver, redigering av eksisterende arkiver og en raskere
én-gangsleser for store uttrekk står igjen som separate trinn.

## 0.19.0: password-protected ZIP

Native password entry and memory-only identity-bound cache for ZipCrypto and
WinZip AES-128/256 reading/extraction. Explicit forget and cancel, bounded retry,
no password persistence. Encrypted 7z/RAR, ZIP header encryption and unusual
compression combinations require another adapter. Creating encrypted archives,
split volumes and archive editing remain open. See ARCHIVES-0.19.0.md.

## 0.20.0 and 0.21.0: native 7z/RAR and multipart inputs

Encrypted 7z/RAR4/RAR5 reading was delivered privately in 0.20.0 with optional
p7zip. 0.21.0 adds numbered 7z, modern RAR and old RAR volume sets, with the same
password workflow and atomic whole-archive publication. See ARCHIVES-0.21.0.md.
Next: encrypted ZIP/7z creation, editing existing archives, then Workbench.
Split ZIP, split creation and remote/nested archive materialization remain open.

## 0.22.0: encrypted archive creation

ZIP AES-256 and 7z AES-256 creation use native library interfaces inside Listi.
7z can additionally encrypt member names. Creation passwords are passed from the
masked confirmation fields to one queued job through a bounded process-local
token and are never serialized to settings or job messages. Editing existing
archives, split creation and encrypted RAR creation remain separate work.

## 0.23.0: safe archive editing

Local, single-volume ZIP and 7z archives can be rebuilt transactionally to add,
replace or remove entries. Source and archive identities are frozen before
confirmation. The replacement is published atomically only after extraction,
editing, writing and flushing succeed. Supported ZIP/7z encryption is retained.
Split, remote, nested and RAR editing remain separate work. See
ARCHIVES-0.23.0.md.

## 0.24.0: Arbeidsbenk

Det fjerde godkjente arbeidsoppsettet bygges av de eksisterende, kvalifiserte
komponentene. Arbeidsbenk har to paneler og starter med begge mappetrær og
forhåndsvisningen synlig. Filter, adresser, handlingsbank, søk og operasjonslogg
er tilgjengelige uten nye filoperasjoner i samme versjon. Bytte mellom oppsett
beholder panelobjekter, mapper, utvalg, delere og pågående jobber. Commander og
Arbeidsbenk lagrer tre- og forhåndsvisningsvalg separat. Se
WORKSPACES-0.24.0.md.

## Long-term portability direction

Listi remains a native Haiku application. Portability is developed gradually,
without replacing the BeAPI interface or holding Deila's peer trials back until
a complete Listi port exists.

- Extract a portable `listi-core` from already tested file-operation, archive,
  provider and identity-guard logic. The core must not include BeAPI or Tracker
  types.
- Keep Haiku-specific UI, BFS attributes, Tracker clipboard, Trash, launch and
  file-type integration behind adapters.
- Prove the core first on Linux and BSD, then add Windows and macOS adapters for
  their different path, metadata, clipboard, Trash and application-launch
  semantics.
- Preserve a native UI per platform unless measured maintenance cost justifies
  a shared shell. Do not introduce Electron merely to obtain portability.
- Use the separate standalone Deila browser to test cross-platform peer sharing
  before optional `deila://` integration or a full Listi port.

Porting claims require native package, filesystem and UI tests on each target;
successful compilation alone is not acceptance.
