# Tools om de Tweede Kamer open data te gebruiken
bert hubert / bert@hubertnet.nl / https://berthub.eu/

Om te beginnen grote dank aan het [Team Open Data van Tweede Kamer](https://opendata.tweedekamer.nl/) voor het
beschikbaar stellen van zulke overweldigende hoeveelheden open data!  Ook
hulde voor de activisten (zoals bij [Openstate](https://openstate.eu) en [1848.nl](https://1848.nl)) die jaren lang hebben
geijverd dat de data er ook zou komen en dat deze goed werd.  Mooi werk! 
Dit project bouwt voort op al dit moois.

De hoop van dit 100% open project is om de gegevensstromen en parlementaire
activiteiten van kabinet en Tweede Kamer zo inzichtelijk mogelijk te maken,
voor zoveel mogelijk mensen.

En niet alleen voor clubs die professionele hulp kunnen betalen. Want er is
al het een en ander, zoals [ANP Politieke
Monitor](https://www.anp.nl/diensten/6/politieke-monitor),
[Polpo](https://polpo.nl/) en [Haagse Feiten](https://haagsefeiten.nl/).

Dit alles zowel voor onderzoeksdoeleinden, maar ook voor iedereen die graag
invloed wil uitoefenen, of zo vroeg mogelijk wil of moet weten wat er gaande
is.

Het is enorm nuttig om nu al te weten dat er over drie maanden een
vergadering is belegd over een onderwerp wat je aangaat.  Op dit moment
komen zelfs professionals daar vaak pas veel te laat achter, en dan is alles
ineens moeilijk.

De Tweedekamer.nl website zelf biedt beperkte mogelijkheden om per email
alerts te krijgen. Het doel van dit project is om dit veel fijnmaziger te
kunnen doen, met veel meer toegesneden alerts.

Er zijn nog wat nevendoelen. De zoekmachine op tweedekamer.nl is niet
bijzonder handig, en er zijn documenten die wel online zijn, maar niet
gevonden worden (helaas).

Ook is er in de database veel meer informatie over belangrijke documenten
dan de tweedekamer.nl website laat zien. Dit geldt met name voor documenten
van de kamer zelf. Deze worden wel weergegeven op de site, maar er staat
niet bij waar het document bijhoort, welke vergadering, welke activiteit.

# Met dank aan

Veel dank aan Jan Allersma, Peter Peerdeman en Bart Schuller voor hun open
source bijdragen!  Verder bouwt dit project op de noeste arbeid van de staf
van de Tweede Kamer, zonder hun werk was er niks weer te geven.

De software leunt verder zwaar op [SQLite](https://sqlite.org), [Pico CSS](https://picocss.com/),
[Pugixml](https://pugixml.org/), [nlohmann-json](https://json.nlohmann.me/),
[cpp-httplib](https://github.com/yhirose/cpp-httplib) en
[fmt](https://fmt.dev).

# Bronnen
Op https://opendata.tweedekamer.nl/ is alle informatie te vinden over de
mooie open data van de Tweede Kamer.

Deze is beschikbaar als een XML feed. Er is ook een JSON API. De XML feed
bevat alle data die je ooit nodig hebt, maar is nog geen database.

Dit project bestaat uit de volgende tools:

 * tkgetxml: volgt de SyncFeed API en slaat XML entries op voor alle
   categorieen data
 * tkconv: zet de meeste typen entries om tot regels in een sqlite database, en voert ook onderhoud op om gewiste documenten ook echt te verwijderen. Voert ook wat zwaardere queries uit zodat ze klaar zijn voor tkserve (zie beneden).
 * tkpull: haalt de 'enclosures' uit de entries met daarin documenten op
 * tkindex: indexeert alle Document entries waarvan we een enclosure hebben
 * tkserve: stelt de data uit de sqlite database beschikbaar, en voert
   zoekslagen uit op de database gemaakt door tkindex
 * tkbot: nog experimenteler dan de rest, detecteert "nieuwe" documenten

# Pagina's

 * index.html: nieuwste documenten, minus de bijlagen 
 * search.html: zoeken
 * geschenken.html: de nieuwste geschenken
 * ongeplande-activiteiten.html: activiteiten nog zonder datum
 * activiteiten.html: komende activiteiten, met datum
 * toezeggingen.html: alle openstaande toezeggingen van regering

# Syntax voor de zoekmachine
Losse woorden: moeten allemaal aanwezig zijn, maar niet noodzakelijk naast elkaar.

"Ieder" "woord" "mag" ook tussen aanhalingstekens. Dit is verplicht bij "woorden-met-een-streepje". Als je zoekt op alleen F-35 dan voegt de software de " voor je toe.

Als je twee woorden bij elkaar in de buurt wil, zoek dan op NEAR(prof Smeeets)

Woorden kunnen geschakeld worden met AND, OR en NOT. Bijvoorbeeld: Hubert NOT Bruls

Ook kan er gezocht worden op prefixes, bijvoorbeeld: HOL0\* 

# Toegang tot de data
Je kan de software zelf compileren en draaien en dan haalt hij alles op bij
de Tweede Kamer. Ik kan je ook een kopie geven van de sqlite database zodat
je je eigen onderzoek kan doen zonder software. Weet me te vinden!
bert@hubertnet.nl - let wel op, m'n gratis hulp is alleen voor organisaties
van publiek belang. 

# Gebruik van de software

## via docker

Voor een snelle start met gecompileerde software kan je ook gebruiken maken van
een docker container, waarin alle afhankelijkheden reeds geinstalleerd zijn.
Zorg dat docker [geinstalleerd is](https://docs.docker.com/engine/install/),
clone deze repository en gebruik de volgende commandos om de image te
builden en de gecompileerde bestanden uit te voeren:

```
docker build --tag tkconv .
docker run -it -v $PWD:/app --rm tkconv:latest tkgetxml
docker run -it -v $PWD:/app --rm tkconv:latest tkconv
docker run -it -v $PWD:/app -p 8089:8089 --rm tkconv:latest tkserv
```

## zelf vanuit source compileren
Als je de boel zelf wil draaien heb je ongeveer zoveel data nodig:
```
9.2G	xml.sqlite3
2.5G	tk.sqlite3
16G	tkindex.sqlite3
28G	total
138G	docs
```

Vergt een moderne linux/unix met diverse dependencies:

```bash
apt-get install nlohmann-json3-dev libsqlite3-dev libpugixml-dev libssl-dev \
zlib1g-dev poppler-utils catdoc imagemagick \
pkg-config xmlstarlet
```

Begin met: meson setup build
En dan bouwen als: meson compile -C build

Ook is de nieuwste versie van pandoc nodig in productie, nieuwer dan in
Debian Bookworm. [3.5 werkt
goed](https://github.com/jgm/pandoc/releases/tag/3.5).

## Lokaal draaien

Alles wil draaien vanuit de root directory van het project.

Draai ./build/tkgetxml en ./build/tkconv eerst met de hand, en doe daarna:

```bash
sqlite3 tk.sqlite3 < maak-indexen
```

En daarna voor productie:

```bash
while true; do ./build/tkgetxml ; ./build/tkconv  ; ./build/tkpull;  ./build/tkindex; sleep 60; done
```

En parallel:

```bash
./build/tkserv
```

Voor de website. tkserv is een eigen webserver op poort 8089 of een andere
poort als je die opgeeft op de commandline. Aanrader is om er bijvoorbeeld
nginx voor te zetten voor de TLS.

# Architectuur
Vrijwel al het zware werk wordt gedaan door sqlite3, inclusief de
zoekmachine. Intern is er een module die SQLite antwoorden omzet in JSON. 

De websites zijn, op 1 pagina na, aangedreven door JSON queries naar tkserv.
De ene uitzondering is in de /get/ pagina om documenten mee op te vragen.
Dit is vanwege opengraph, waarbij social media geen javascript gaan runnen
voor een link card.

# Meedoen
Het is expliciet de hoop dat dit een actief open source project wordt.
Tegelijk is er gekozen voor een vrij specifieke architectuur. Als je graag
had gewild dat tkconv veel ingewikkelder was, maar wel met meer gangbare
tools, dan is dit mogelijk niet het project voor jou.

# Hoe het samenhangt
Er zijn nogal wat 'categories' aan informatie, en ze kunnen ook op allemaal
manieren samenhangen.

## Document
Een Document kan op zichzelf staan, maar is meestal onderdeel van een Zaak,
or zelfs van meerdere Zaken. Een Zaak ondertussen brengt bijvoorbeeld het
Document met een schriftelijke vraag samen met het Document met het
schriftelijk antwoord.

Ondertussen kunnen Documenten weer bijlagen hebben, te vinden omdat zo'n
bijlage een bronDocument veld heeft. 

De boom is dan dus Zaak -> Documenten -> bijlagen (ook Documenten).

Een Document kan ook onderdeel zijn van een Kamerstukdossier, en neemt daar
dan ene specifiek volgnummer in.

## Zaak
Een Zaak is soms alleen en enveloppe om een Document heen, zonder veel toe
te voegen. Maar vaak brengt het ook twee Documenten samen. Een Zaak kan ook
weer behoren tot een kamerstukdossier.

## Activiteit
### Agendapunt


## Besluit
### Stemming
