# Arbeidsplan for Listi

14. november 2026. Første mål er en god, særpreget filbehandler til daglig bruk.
Vi leverer små, ferdige arbeidsflyter som samtidig peker mot den større visjonen.
Vi bygger videre på 0.2.0. Ingen stor omskriving er en forutsetning.

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

## Neste små leveranser, prioritert etter faktisk bruk

- Bokmerker og rask navigasjon, bedre forhåndsvisning og filtypehandlinger.
- Batch-navneendring med forhåndsvisning, når grunnflyten er solid.
- Felles handlinger for menyer, knapper og snarveier. Deretter redigerbare
  knappebanker og en kommandopalett som viser mulighetene, ikke skjuler basisbruk.
- Flere lister, låste roller og lagrede oppsett når konkrete arbeidsoppgaver
  viser hvordan de skal fungere. Både tastatur og mus skal ha en naturlig flyt.

## Senere visjoner som første etappe skal tåle

- BFS-attributter og levende søk som arbeidsflater. Vis originalens plassering;
  sletting av treff er sletting av originalen. Lås filutvalget ved jobbstart.
- Arkiver, sammenligning/synkronisering med forhåndsvisning og nettverkskilder i
  separate steg. SMB/SFTP/FTP krever egne avbrudds- og credentialvurderinger.
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
