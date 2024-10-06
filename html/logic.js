function recentInit(f)
{
    setInterval(getRecentDocs, 60000, f);
    getRecentDocs(f);
}

async function getRecentDocs(f) {
    
    const response = await fetch('recent-docs');
    if (response.ok === true) {
        const data = await response.json();
        f.recentDocs = data;
	console.log("Update!");
    }
}

function recentInit2(f)
{
    setInterval(getRecentDocs2, 60000, f);
    getRecentDocs2(f);
}

async function getRecentDocs2(f)
{
    const formData = new FormData();
    formData.append('onlyRegeringsstukken', f.onlyRegeringsstukken);
    console.log(f.onlyRegeringsstukken);
    const response = await fetch('recent-docs', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
        f.recentDocs = data;
	console.log("Update!");
    }

    const response2 = await fetch('jarig-vandaag');
    if (response2.ok === true) {
        const data = await response2.json();
        f.jarigVandaag = data;
    }
}


async function getRecentActiviteiten(f) {
    
    const response = await fetch('future-activities');
    if (response.ok === true) {
        const data = await response.json();
        f.recentActiviteiten = data;
    }
}

async function getOngeplandeActiviteiten(f) {
    
    const response = await fetch('unplanned-activities');
    if (response.ok === true) {
        const data = await response.json();
        f.ongeplandeActiviteiten = data;
    }
}

async function getGen(orig, dest, f) {
    
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
    }
}

async function getGenKSD(orig, dest, f)
{
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
	var ksdnamen=[]
	for (const element of data) {
	    if(element.toevoeging != '')
		ksdnamen.push(element.nummer +"-" + element.toevoeging +" " +element.titel);
	    else
		ksdnamen.push(element.nummer +" " +element.titel);
	}
	config= {
	    placeHolder: "Kamerstukdossier titel",
	    data: {
		src: ksdnamen
	    },
	    diacritics: true,

	    resultItem: {
		highlight: {
		    render: true
		},
		submit: true
	    },
	    resultsList: {
		maxResults: 100,
		element: (list, data) => {
		    const info = document.createElement("p");
		    if (data.results.length) {
			info.innerHTML = `Displaying <strong>${data.results.length}</strong> out of <strong>${data.matches.length}</strong> results`;
		    } else {
			info.innerHTML = `Found <strong>${data.matches.length}</strong> matching results for <strong>"${data.query}"</strong>`;
		    }
		    list.prepend(info);
		    
		}
	    },
	    events: {
		input: {
		    selection: (event) => {
			const selection = event.detail.selection.value;
			autoCompleteJS.input.value = selection;
			selected = selection.split(" ")[0];
			// https://berthub.eu/tkconv/ksd.html?ksd=25424
			fspl = selected.split("-");
			const num = fspl[0];
			let toevoeging="";

			if(fspl.length > 1)
			    toevoeging = fspl[1];
			window.location.href = "ksd.html?ksd="+selected+"&toevoeging=" + toevoeging;
		    }
		}
	    }
	};
    }
    const autoCompleteJS = new autoComplete(config);
}

async function getGenVragen(orig, dest, f)
{
    const response = await fetch(orig);
    if (response.ok === true) {
        const data = await response.json();
        f[dest] = data;
	var ksdnamen=[]
	for (const element of data) {
	    ksdnamen.push(element.nummer +" " + element.onderwerp);
	}
	config= {
	    placeHolder: "Vraag",
	    data: {
		src: ksdnamen
	    },
	    diacritics: true,

	    resultItem: {
		highlight: {
		    render: true
		},
		submit: true
	    },
	    resultsList: {
		maxResults: 1000,
		element: (list, data) => {
		    const info = document.createElement("p");
		    if (data.results.length) {
			info.innerHTML = `Displaying <strong>${data.results.length}</strong> out of <strong>${data.matches.length}</strong> results`;
		    } else {
			info.innerHTML = `Found <strong>${data.matches.length}</strong> matching results for <strong>"${data.query}"</strong>`;
		    }
		    list.prepend(info);
		    
		}
	    },
	    events: {
		input: {
		    selection: (event) => {
			const selection = event.detail.selection.value;
			autoCompleteJS.input.value = selection;
			selected = selection.split(" ")[0];
			// https://berthub.eu/tkconv/ksd.html?ksd=25424
			window.location.href = "zaak.html?nummer="+selected;
		    }
		}
	    }
	};
    }
    const autoCompleteJS = new autoComplete(config);
}



async function getOpenToezeggingen(f) {
    
    const response = await fetch('open-toezeggingen');
    if (response.ok === true) {
        const data = await response.json();
        f.openToezeggingen = data;
    }
}

function orderByDate(f)
{
    console.log("We gaan sorteren!");
    f.foundDocs=f.foundDocs.sort(function(a,b) {
	if(a.datum < b.datum)
	    return 1;
	if(a.datum > b.datum)
	    return -1;
	return 0;
    });
}

function orderByScore(f)
{
    f.foundDocs=f.foundDocs.sort(function(a,b) {
	return a.score - b.score;
    });
}

// for search
function init(f)
{
    let url = new URL(window.location.href)
    f.searchQuery = url.searchParams.get("q");
    if(url.searchParams.get("twomonths")=="true")
	f.twomonths=true;
    else
	f.twomonths=false;
    if(url.searchParams.get("soorten") != null)
	f.soorten = url.searchParams.get("soorten");
    else
	f.soorten= "alles";
    console.log(f.soorten);
    if(f.searchQuery != null)
	getSearchResults(f);
}

async function commissieinit(f)
{
    let url = new URL(window.location.href)
    f.id = url.searchParams.get("id");
    const response = await fetch('commissie/'+f.id);
    if (response.ok === true) {
        const data = await response.json();
	f["data"] = data;
	f.loaded=true;
    }
}


function ksdinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("ksd");
    f.toevoeging = url.searchParams.get("toevoeging");
    getKSDDocs(f);
}

async function getKSDDocs(f)
{
//    const url = new URL(window.location.href);
 //   url.searchParams.set("ksd", f.nummer);
   // history.pushState({}, "", url);

    const response = await fetch('ksd/'+f.nummer+'/'+f.toevoeging);
    if (response.ok === true) {
        const data = await response.json();
        f["docs"] = data["documents"];
	f["meta"] = data["meta"];
    }
}

function persooninit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("nummer");
    getPersoon(f);
}

async function getPersoon(f)
{
    const response = await fetch('persoon/'+f.nummer);
    if (response.ok === true) {
        const data = await response.json();
	f["data"] = data;
	f.loaded = true;
    }
}


async function verslaginit(f)
{
    let url = new URL(window.location.href)
    let vergaderingid = url.searchParams.get("vergaderingid");
    const response = await fetch('vergadering/' + vergaderingid);
    if (response.ok === true) {
        const data = await response.json();
	f["meta"] = data;
	f["loaded"] = true;
    }
}

async function zaakinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("nummer");
    getZAAKDocs(f);
}

async function getZAAKDocs(f)
{
//    const url = new URL(window.location.href);
 //   url.searchParams.set("zaak", f.nummer);
   // history.pushState({}, "", url);

    const response = await fetch('zaak/'+f.nummer);
    if (response.ok === true) {
        const data = await response.json();
        f["zaak"] = data;
	console.log(data);
    }
}

async function activiteitinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("nummer");
    return getActiviteitDetails(f);
}

async function getActiviteitDetails(f)
{
//    const url = new URL(window.location.href);
 //   url.searchParams.set("zaak", f.nummer);
   // history.pushState({}, "", url);

    const response = await fetch('activiteit/'+f.nummer);
    if (response.ok === true) {
        const data = await response.json();
        f["activiteit"] = data;
	console.log(data);
	f["loaded"]=true;
    }
}



async function getSearchResults(f)
{
    if(f.searchQuery == '' || f.searchQuery == null)
	return;
    const url = new URL(window.location.href);
    url.searchParams.set("q", f.searchQuery);
    url.searchParams.set("twomonths", f.twomonths);
    url.searchParams.set("soorten", f.soorten);
    history.pushState({}, "", url);
    f.alternatief='';
    f.message='';
    if(/2[0-9][0-9][0-9]Z[0-9]*/.test(f.searchQuery)) {
	console.log("zaak match");
	f.alternatief = `<p><em>Bedoelt u mogelijk zaak <a href="zaak.html?nummer=${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    if(/2[0-9][0-9][0-9]A[0-9]*/.test(f.searchQuery)) {
	console.log("zaak match");
	f.alternatief = `<p><em>Bedoelt u mogelijk activiteit <a href="activiteit.html?nummer=${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    
    else if(/2[0-9][0-9][0-9]D[0-9]*/.test(f.searchQuery)) {
	f.alternatief = `<p><em>Bedoelt u mogelijk document <a href="get/${f.searchQuery}">${f.searchQuery}</a>?</em></p>`;
    }
    else if(/^[0-9][0-9][0-9][0-9][0-9]$/.test(f.searchQuery)) {
	f.alternatief = `<p><em>Bedoelt u mogelijk kamerstukdossier <a href="ksd.html?ksd=${f.searchQuery}&toevoeging=">${f.searchQuery}</a>?</em></p>`;
    }
    
    const formData = new FormData();
    formData.append('q', f.searchQuery);
    formData.append('twomonths', f.twomonths);
    formData.append('soorten', f.soorten);

    const response = await fetch('search', { method: "POST", body: formData });
    if (response.ok === true) {
        const data = await response.json();
        f.foundDocs = data["results"];
	
	f.message = `${data["milliseconds"]} milliseconden`;
	console.log(data);
    }
    else {
	f.foundDocs=[];
	f.message = `Geen resultaten - probeer "${f.searchQuery}"`;
    }
}
