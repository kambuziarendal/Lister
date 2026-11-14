# Arbeidsplan for Listi

14. november 2026. Første mål er en god, særpreget filbehandler til daglig bruk.
Vi leverer små, ferdige arbeidsflyter som samtidig peker mot den større visjonen.
Vi bygger videre på 0.2.0. Ingen stor omskriving er en forutsetning.

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

### Aktivt trinn og kø

1. **Aktivt: filoperasjonenes sikkerhet og modulgrenser.** Gjennomgå dispatch,
   konflikter, avbrudd, kildeendringer og ferdigmarkering. Test uten å avhenge av UI.
2. Arkiv- og nettverkspaneler: fullfør SMB, arkiver, SFTP/FTP og tydelige
   feil/kapabiliteter. Eksisterende kode er en kandidat, ikke ferdig funksjonsdekning.
3. Panelvisninger: detaljer, ikoner og thumbnails, valgfri bildevisning,
   mappevalg og uavhengige mappetrær. Test fokus, utvalg og kompakt laptopoppsett.
4. Intern operasjonsvisning: status, konflikter, per-fil fremdrift og avbrudd.
   Grønt betyr bekreftet ferdig, ikke bare at data er lest eller sendt.
5. Samlet intern kandidat: installasjon, bevart konfigurasjon og brukerflyt på Haiku.
6. Kartlegg Dolphin-funksjoner og prioriter neste konkrete bruksbehov fra matrisen.

Tilpasning skal følge prinsippet i DESIGN.md: gode standardvalg, lokale
panelkontroller, eksplisitte mappevalg og en enkel vei tilbake. Lagrede
arbeidsoppsett som «Laptop» og «Stor skjerm» ligger i kø, ikke i aktivt trinn.

Påbegynt UI-kode i punkt 3 og 4 beholdes, men utvides ikke mens punkt 1 sikres.
Ingen av disse punktene er automatisk autorisasjon til offentlig publisering.

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

## Retningsprøven

Hvert steg skal gjøre en konkret oppgave bedre i dag, passe naturlig på Haiku og
gi plass til videre utvikling. DOpus gir handlefriheten, moderne arbeidsmåter
former flyten. Vi trenger ikke alt samtidig for å ha en tydelig egen identitet.
