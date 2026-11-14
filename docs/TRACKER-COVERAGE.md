# Tracker-dekning på Listis egne premisser

Arnes krav, 14. november 2026: Alt Tracker tilbyr skal også kunne gjøres i Listi,
med vår egen kode, utforming og arbeidsflyt. Dette kommer i tillegg til
[Dolphin-kartet](FUNCTION-COVERAGE.md), ikke i stedet for det. Sameksistens med
Tracker er fortsatt standard. Å stoppe Tracker eller erstatte skrivebordet er
ikke godkjent av dette funksjonskravet.

## Referanse og status

Første kart bygger på Haikus offisielle brukerhåndbok hentet 14. november 2026.
URL, tidspunkt og innholdshash er bevart i
[referansekvitteringen](reference/tracker-userguide-20261114.json). Håndboken er
løpende dokumentasjon, ikke en versjonsfestet Tracker-binær. Testmiljøet vårt er
Haiku beta6 hrev59866+79 x86_64. Tracker er ikke funksjonstestet i sin helhet her.
Kravlisten må utvides ved runtime-gjennomgang og nye dokumenterte funksjoner.

Listi-basis: privat 0.5.0, med avgrenset testbevis i NAVIGATION-0.5.0.md og
ACCEPTANCE-0.4.0.md. D betyr delvis dekning, M betyr mangler egen brukerflyt,
U betyr tilgjengelig mekanisme, men behovet er ikke tilstrekkelig verifisert.
Oppdaterte søketrinn er versjonsmerket til 0.8.0, se ACCEPTANCE-0.8.0.md.
Lokal filutklippstavle er kvalifisert i 0.9.0, se ACCEPTANCE-0.9.0.md.
Egenskaper og typed BFS-attributter er avgrenset i PROPERTIES-0.10.0.md.
Ingen rad nedenfor påstår full Tracker-likhet. Systemintegrasjon teller først
når den kan startes fra Listi og hele oppgaven er prøvd.

| ID | Brukerbehov | Listi | Neste akseptprøve / udekket del | Kilde |
|---|---|---|---|---|
| T01 | Navigere mappehierarki, opp og direkte sti | D | Forelder, tre og sti finnes. Full keyboard/pane-prøve | Tracker |
| T02 | Tilbake/Frem og flere arbeidsflater | D | Historikk per panel testet 0.5.0. Flere vinduer/faner mangler | Tracker |
| T03 | Velge mål i underliggende mapper under drag | M | Utforsk mål uten å miste fryst kildeutvalg | Tracker |
| T04 | Type-ahead på navn, deretter attributter | D | Navnefilter finnes, attributtsøk og hopp mangler | Tracker |
| T05 | Vedvarende flerleddsfilter og nullstilling | D | Ett navnefilter finnes, flere søkeledd mangler | Tracker |
| T06 | Skjule og vise punktfiler | M | Eget valg, bevart per arbeidsflate | Tracker |
| T07 | Små/store ikoner og detaljvisning | D | Visninger finnes, zoom og ønsket sortering må utvides | Tracker |
| T08 | Miniatyrbilder fra systemets bildeformater | D | Bilder testet. Flere oversettere/formater og feilbilder | Tracker |
| T09 | Manuell plassering, justering og etikettvalg | M | Egen organisering, ikke kopi av Trackers ikonlayout | Tracker |
| T10 | Sortering stigende/synkende med sekundærnøkkel | D | Detaljkolonner finnes, full flerfeltssortering uprøvd | Attributes |
| T11 | Valgfrie og flyttbare attributtkolonner | M | Typer, bredder, rekkefølge og lagring per mappe | Attributes |
| T12 | Vise og redigere typed BFS-attributter | D (0.10) | Åtte lokale datatyper, validering og utkastvern. Systemattributter/binærdata, flere typer og egne People/mail/audio-flyter gjenstår | Attributes |
| T13 | Velge etter mønster eller regulært uttrykk | M | Fryst utvalg, ugyldig uttrykk, tomt resultat | Tracker |
| T14 | Vanlige flerutvalg, tastatur og drag | D | Egen liste/grid testet. Alle modifierkombinasjoner | Shortcuts |
| T15 | BFS-søk på navn, typer, attributter og formler | D (0.7) | Lokalt navnesøk og native BFS-formler som snapshot; attributtbygger gjenstår | Queries |
| T16 | Velge søkevolumer, mapper og inkludering av Trash | D (0.7) | Én mappe/volumrot og eksplisitt Trash-valg; flervolumvalg gjenstår | Queries |
| T17 | Levende treff med originale filhandlinger | D (0.8) | Levende BFS med identitetsbevart valg og vis-original i eierpanelet. Filhandlinger går via panelet; større datamengder/providere gjenstår | Queries |
| T18 | Lagrede søk, søkemaler og gjenåpning/redigering | D (0.8) | Listi-kriterier lagres, redigeres og kjøres på nytt. Tracker query-filer, import/eksport og maler gjenstår | Queries |
| T19 | Tidsuttrykk og logiske AND/OR-vilkår | D (0.7) | Native AND/OR-formler prøvd. Dato-/tidssonegrenser og vennlig byggeverktøy gjenstår | Queries |
| T20 | Opprette mapper og filer fra maler | D | Ny mappe finnes. Filmaler, metadata og malmapper mangler | Tracker |
| T21 | Åpne med og velge foretrukket program | D | 0.11: engangsvalg, native programvelger og MIME-visning prøvd i installert pakke. Lagrede per-fil/type-standarder gjenstår | Filetypes |
| T22 | Filegenskaper, plassering, eier og rettigheter | D (0.10) | Lokal inspeksjon og vanlige oktale rettigheter. Eierbytte, spesialbiter, rekursiv størrelse og virtuelle filkilder gjenstår | Tracker |
| T23 | MIME-identifikasjon og korrigering av filtype | D | 0.11: lagret eller oppdaget MIME vises uten skriving. Korrigering/force-identify gjenstår | Tracker |
| T24 | Egendefinerte ikoner og filtypeinnstillinger | M | Native integrasjon, per-fil kontra globalt valg | Filetypes |
| T25 | Kopiere, flytte, duplisere og endre navn | D | Sikker kø og lokal duplisering (0.13). Bred metadata-matrise gjenstår | Tracker |
| T26 | Absolutte og relative lenker, åpne lenkemål | D | Lokal oppretting av relative/absolutte lenker (0.13). Målflyt gjenstår | Tracker |
| T27 | Filutklippstavle med cut/copy/paste | D (0.9) | Native lokale referanser, begge veier med Tracker, kilde-/målidentitet og avbrudd prøvd. Virtuelle kilder og vedvarende cut-markering gjenstår | Tracker |
| T28 | Utvide utklippstavleutvalg og lime inn som lenker | M | Egne synlige valg uten skjulte tastaturkrav | Tracker |
| T29 | Trash, gjenoppretting og permanent sletting | D | Grunnflyt testet. Volumer, fulle rettigheter og flere konflikter | Tracker |
| T30 | Angre og gjør om filhandlinger | M | Transaksjonsjournal, endrede mål og sikker gjenoppretting | Shortcuts |
| T31 | Fremdrift, pause og avbrudd for samtidige jobber | D | Kø testet. Egen kontroll per jobb og stresstest | Tracker |
| T32 | Monterte volumer, ledig plass og kapasitetsstatus | D | Diskliste finnes. Tydelig kapasitet/volumstatus mangler | Tracker |
| T33 | Montere skrivebeskyttet, avmontere og løse ut | M | Åpne filer, pågående jobber og databevarende feilflyt | Tracker |
| T34 | Automontering og valg ved oppstart/tilkobling | M | Native integrasjon, ingen skjult global endring | Tracker |
| T35 | Tillegg filtrert på MIME og gjeldende utvalg | M | Isolert kjøring, feiltillegg, ingen kodekopiering | Add-ons |
| T36 | DiskUsage, TextSearch, FileType og ZipOMatic | M | Sammenhengende Listi-startede native oppgaver | Add-ons |
| T37 | Bakgrunn per mappe og skrivebord | M | Egen løsning eller eksplisitt native integrasjon | Add-ons |
| T38 | Skrivebordsfiler, vinduer og workspace-handlinger | D | Desktop-mappe tilgjengelig. Skrivebordsvert ikke implementert | Tracker/Shortcuts |
| T39 | Lagrede mappeoppsett, ideell størrelse og layoutgjenbruk | D | Lagret visningsmodus finnes. Kolonner/geometri/maler mangler | Tracker/Queries |
| T40 | Metadata bevares i kopiering og malbruk | D | Lokal motor har attributter. Kryssvolum/formatgrenser må dokumenteres | Attributes/Tracker |

## Arbeidsrekkefølge

1. Fullfør lagrede arbeidssteder og den bestilte bughunten.
2. Søk må dekke både vanlig mappe-/navnesøk og Haikus BFS-behov. Ikke kall
   navnefilteret en erstatning for Queries.
3. Attributter, egenskaper, filtyper, åpne-med og interoperabel filutklippstavle.
4. Angre/journal, maler, lenker, volumhandlinger og native tilleggsflyter.
5. Systematisk Tracker-runtime-gjennomgang før noen påstand om full dekning.
   Skrivebordsvert og eventuell Tracker-erstatning krever egen plan og recovery.

Prioriteringen kan endres etter faktisk bruk. Utsettelse fjerner ikke kravet.
Vi kopierer ikke Tracker-kode, grafikk, menystruktur eller arbeidsflyt.

## Offisielle kilder

- [Tracker](https://www.haiku-os.org/docs/userguide/en/tracker.html)
- [Attributes](https://www.haiku-os.org/docs/userguide/en/attributes.html)
- [Queries](https://www.haiku-os.org/docs/userguide/en/queries.html)
- [Filetypes](https://www.haiku-os.org/docs/userguide/en/filetypes.html)
- [Add-ons](https://www.haiku-os.org/docs/userguide/en/tracker-add-ons.html)
- [Shortcuts](https://www.haiku-os.org/docs/userguide/en/keyboard-shortcuts.html)
