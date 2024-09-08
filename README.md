# Tools om de Tweede Kamer open data te gebruiken
Om te beginnen grote dank aan de Tweede Kamer voor het beschikbaar stellen
van zulke overweldigende hoeveelheden open data. Ook hulde voor de
activisten (zoals bij Openstate) die jaren lang hebben geijverd dat de data
er ook zou komen en dat deze goed werd. Mooi werk! Dit project bouwt voort
op al dit moois.

De hoop van dit project is om de gegevensstromen en activiteiten van kabinet
en Tweede Kamer zo inzichtelijk mogelijk te maken, voor zoveel mogelijk
mensen. En niet alleen voor clubs die professionele hulp kunnen betalen. 

Dit alles zowel voor onderzoeksdoeleinden, maar ook voor iedereen die graag
invloed wil uitoefenen, of zo vroeg mogelijk wil of moet weten wat er gaande
is.

Het is enorm nuttig om te weten dat er over drie maanden een vergadering is
belegd over een onderwerp wat je aangaat. Nu komen zelfs professionals daar
vaak pas veel te laat achter, en dan is alles ineens moeilijk.

De Tweedekamer.nl website zelf biedt beperkte mogelijkheden om per email
alerts te krijgen. Het doel van dit project is om dit veel fijnmaziger te
kunnen doen, met veel meer relevante alerts.

Er zijn nog wat nevendoelen. De zoekmachine op tweedekamer.nl is niet
bijzonder handig, en er zijn documenten die wel online zijn, maar niet
gevonden worden. 

Ook is er in de database veel meer informatie over belangrijke documenten
dan de tweedekamer.nl website laat zien. Dit geldt met name voor documenten
van de kamer zelf. Deze worden wel weergegeven op de site, maar er staat
niet bij waar het document bijhoort, welke vergadering, welke activiteit.

# Bronnen
Op https://opendata.tweedekamer.nl/ is alle informatie te vinden over de
mooie open data van de Tweede Kamer.

Deze is beschikbaar als een XML feed. Er is ook een JSON API. De XML feed
bevat alle data die je ooit nodig hebt, maar is nog geen database.

Dit project bestaat uit de volgende tools:

 * tkgetxml: volgt de SyncFeed API en slaat XML entries op voor alle
   categorieen data
 * tkconv: zet de meeste typen entries om tot regels in een sqlite database
 * tkpull: haalt de 'enclosures' uit de entries met daarin documenten op
 * tkindex: indexeert alle Document entries waarvan we een enclosure hebben
 * tkserve: steelt de data uit de sqlite database beschikbaar, en voert
   zoekslagen uit op de database gemaakt door tkindex


# Open
2024D30849 - de kamer website weet de oorspronkelijke vraag hierbij te
vinden, hoe dan?

2024D31742 - kamer website komt van dit antwoord naar de kamervraag, hoe
8405e3ca-f7cb-49ca-ac51-aef4d4eb780f

# motie
Een motie is een Document, met ALTIJD een kamerstukdossierid


# cleanup 
hoe dan? alleen nieuwste versie van een id bewaren
