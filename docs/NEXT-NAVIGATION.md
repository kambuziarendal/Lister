# Opprinnelig byggeplan: finn tilbake i hvert panel

Implementert i privat 0.5.0. Se [resultat og verifikasjon](NAVIGATION-0.5.0.md).
Kravene under beholdes som den opprinnelige avgrensningen.

14. november 2026. Avgrenset plan etter den interne 0.4.0-kandidaten.
Referansebehov: N03 i FUNCTION-COVERAGE.md. Dette er en Listi-løsning, ikke en
bestilling på Dolphins knapper, layout, kode eller sesjonsmodell.

## Brukerresultat

Vi skal kunne følge en mappegren, åpne et arkiv eller besøke en deling, og finne
tilbake til forrige sted uten å skrive stien på nytt. Hvert panel har sin egen
historikk. Et panelbytte eller en filjobb skal ikke forandre det andre panelets
historikk eller jobbens frosne mål.

## Avgrensning

- Tilbake og Frem tilbys lokalt i panelnavigasjonen, med mus og native tastatur.
  Endelig plassering og snarveier velges ut fra Listis eksisterende kontroller,
  tilgjengelig plass og Haikus keymap. Ikke lån referansens tastaturkart ukritisk.
- Historikken gjelder denne økten, med høyst 64 vellykkede steder per panel.
  Ingen lagring av passord, kontodetaljer, filvalg eller jobber. Favoritter og
  gjenoppretting etter omstart er egne senere behov, ikke skjult scope her.
- Bruk eksisterende normalisering av lokale/virtuelle steder. Samme sted og
  Refresh gir ikke nye historikkledd. Forelder, tre, Go, arkiv, tilkobling,
  scripting og «samme mappe i motsatt panel» går gjennom én navigasjonsvei.
- Etter A -> B -> C -> Tilbake til B -> D skal Frem-grenen til C bortfalle.
  Tilbake/Frem skaper ikke nye duplikater. Grenseknapper/-handlinger er inaktive.
- En asynkron mappeavlesning må identifiseres med eksisterende navigasjonsserial.
  Et sent svar skal ikke flytte historikkpekeren eller overskrive nyere navigasjon.
- Mislykket/avbrutt åpning opprettes ikke som et vellykket besøk. Forrige gyldige
  historikkposisjon beholdes. Hvis feilforsøket vises i stifeltet, skal Tilbake først
  returnere til siste gyldige sted, ikke hoppe over det. Ingen automatisk looping
  mellom frakoblede adresser.
- Tilbake til en mappe betyr ny kontroll av dens innhold. Gamle filvalg gjenopplives
  ikke. Eksisterende mappeoverrides gjelder fortsatt, og feil/konflikter i køen
  beholder sitt eierskap. Filterets eksisterende policy ved mappebytte beholdes.
- Ingen favorittdatabase, nytt transportsystem, global kommandopalett, faner,
  automatisk jobbresume eller full ombygging av Pane i dette trinnet.

## Forventede modulgrenser

En liten navigasjonstilstand eier historikk og peker, uavhengig av renderere og
filoperasjoner. Pane eier livsløpet for den faktiske forespørselen og bekrefter
utfallet fra scanner. UI sender hensikter, ikke egne kopier av historikken.

Før implementering kontrolleres Pane::Navigate, kScanDone/serial, tre-navigasjon,
arkivåpning og scripted SourcePath/DestinationPath. Kildegrensen er navigasjon og
relevante kontroller. Ingen endring av filoperasjonenes commit-/rollbackkode er
nødvendig for denne funksjonen.

## Ferdigkrav

1. Tilstandsregresjoner: tom historikk, første sted, A/B/C, tilbake/frem, ny gren,
   samme sted, Refresh, 65 besøk, uavhengige paneler og sent/avbrutt svar.
2. Native Haiku: utfør lokal mappe -> underkatalog -> tilbake/frem med både mus og
   tastatur. Les tilbake begge panelstier, ikke bare knappetekst.
3. Native Haiku: lokal -> ZIP/tar-inngang -> arkivmappe -> tilbake til lokal,
   deretter frem igjen. Bekreft at selve arkivet ikke endres.
4. Eide nettverksfixtures: vellykket SMB/SFTP-sted, frakoblet/avvist mål, tilbake
   til gyldig sted og forsinket svar etter ny navigasjon. Ingen reelle kontoer.
5. Start en ventende kopikonflikt, naviger tilbake/frem og bytt panel. Bekreft
   fortsatt frosset kilde/mål og at et historikkvalg ikke besvarer konflikten.
6. Kontroller filter, tre, mappeoverride, fokus og tekstfeltets vanlige snarveier.
   Tilbake/Frem må ikke stjele tekstredigeringens eller dialogens tastaturhandling.
7. 1024x650, lyst/mørkt, begge trær og preview. Behold lesbarheten når inline-
   spørsmål og operasjonskonflikt er åpne. Test også større valgt font.
8. Én ferdig native kandidat: relevant samlet regresjon, faktisk installasjon og
   bevart oppsett før eventuell privat pakke. Ingen offentlig push eller release.

Funksjonen finnes ikke i den tidligere leverte 0.4.0-pakken. Resultatene fra
0.5.0-kontrollene dokumenteres separat, slik at planen ikke blir et testbevis.
