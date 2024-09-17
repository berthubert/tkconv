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
			window.location.href = "ksd.html?ksd="+selected;
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
    if(f.searchQuery != null)
	getSearchResults(f);
}

function ksdinit(f)
{
    let url = new URL(window.location.href)
    f.nummer = url.searchParams.get("ksd");
    getKSDDocs(f);
}

async function getKSDDocs(f)
{
//    const url = new URL(window.location.href);
 //   url.searchParams.set("ksd", f.nummer);
   // history.pushState({}, "", url);

    const response = await fetch('ksd/'+f.nummer);
    if (response.ok === true) {
        const data = await response.json();
        f["docs"] = data;
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



async function getSearchResults(f)
{
    const url = new URL(window.location.href);
    url.searchParams.set("q", f.searchQuery);
    url.searchParams.set("twomonths", f.twomonths);
    history.pushState({}, "", url);
    
    const formData = new FormData();
    formData.append('q', f.searchQuery);
    formData.append('twomonths', f.twomonths);

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
