// The following JavaScript routines can be called from C++ code (eg. webcalls.cpp).
// Makefile merges these routines into golly.js via the --js-library option.

var LibraryGOLLY = {

$GOLLY: {
    cancel_progress: false,     // cancel progress dialog?
    progstart: 0.0,             // time when jsBeginProgress is called
    statusbar: null             // status bar element
},

// -----------------------------------------------------------------------------

jsAlert: function(msg) {
    alert(Pointer_stringify(msg));
},

// -----------------------------------------------------------------------------

jsConfirm: function(query) {
    return confirm(Pointer_stringify(query));
},

// -----------------------------------------------------------------------------

jsSetBackgroundColor: function(id, color) {
    document.getElementById(Pointer_stringify(id)).style.backgroundColor = Pointer_stringify(color);
},

// -----------------------------------------------------------------------------

jsSetStatus: function(line1, line2, line3) {
    // check if the statusbar element lookup has already been done
    if (!GOLLY.statusbar) {
        // lookup and cache the statusbar element
        GOLLY.statusbar = document.getElementById('statusbar');
    }

    // set the statusbar
    GOLLY.statusbar.value =
        Pointer_stringify(line1) + '\n' +
        Pointer_stringify(line2) + '\n' +
        Pointer_stringify(line3);
},

// -----------------------------------------------------------------------------

jsSetMode: function(index) {
    document.getElementById('mode').selectedIndex = index;
},

// -----------------------------------------------------------------------------

jsSetState: function(state, numstates) {
    var list = document.getElementById('state');
    // may need to update number of options in list
    while (list.length < numstates) {
        // append another option
        var option = document.createElement('option');
        option.text = list.length.toString();
        list.add(option);
    };
    while (list.length > numstates) {
        // remove last option
        list.remove(list.length - 1);
    }
    list.selectedIndex = state;
},

// -----------------------------------------------------------------------------

jsSetRule: function(oldrule) {
    var newrule = prompt('Type in a new rule:', Pointer_stringify(oldrule));
    if (newrule == null) {
        return allocate(intArrayFromString('\0'), 'i8', ALLOC_STACK);
    } else {
        return allocate(intArrayFromString(newrule), 'i8', ALLOC_STACK);
    }
},

// -----------------------------------------------------------------------------

jsGetSaveName: function(currname) {
    var newname = prompt('Save current pattern in given file:', Pointer_stringify(currname));
    if (newname == null) {
        return allocate(intArrayFromString('\0'), 'i8', ALLOC_STACK);
    } else {
        return allocate(intArrayFromString(newname), 'i8', ALLOC_STACK);
    }
},

// -----------------------------------------------------------------------------

jsShowMenu: function(id, x, y) {
    var menu = document.getElementById(Pointer_stringify(id));
    var mrect = menu.getBoundingClientRect();
    // x,y coords are relative to canvas, so convert to window coords
    var crect = Module['canvas'].getBoundingClientRect();
    // note that scrolling is disabled so window.scrollX and window.scrollY are 0
    x += crect.left + 1;
    y += crect.top + 1;
    // if menu would be outside right/bottom window edge then move it
    if (x + mrect.width > window.innerWidth) x -= mrect.width + 2;
    if (y + mrect.height > window.innerHeight) y -= y + mrect.height - window.innerHeight;
    menu.style.top = y.toString() + 'px';
    menu.style.left = x.toString() + 'px';
    menu.style.visibility = 'visible';
},

// -----------------------------------------------------------------------------

jsSetClipboard: function(text) {
    document.getElementById('cliptext').value = Pointer_stringify(text);
},

// -----------------------------------------------------------------------------

jsGetClipboard: function() {
    var text = document.getElementById('cliptext').value;
    if (text == null) {
        return allocate(intArrayFromString('\0'), 'i8', ALLOC_STACK);
    } else {
        return allocate(intArrayFromString(text), 'i8', ALLOC_STACK);
    }
},

// -----------------------------------------------------------------------------

jsTextAreaIsActive: function() {
    if (document.activeElement.tagName == 'TEXTAREA') {
        return 1;
    } else {
        return 0;
    }
},

// -----------------------------------------------------------------------------

jsElementIsVisible: function(id) {
    if (document.getElementById(Pointer_stringify(id)).style.visibility == 'visible') {
        return true;
    } else {
        return false;
    }
},

// -----------------------------------------------------------------------------

jsEnableButton: function(id, enable) {
    var button = document.getElementById(Pointer_stringify(id));
    if (enable) {
        button.disabled = false;
        button.style.color = '#000';
    } else {
        button.disabled = true;
        button.style.color = '#888';
    }
},

// -----------------------------------------------------------------------------

jsEnableImgButton: function(id, enable) {
    var button = document.getElementById(Pointer_stringify(id));
    var img = document.getElementById('img' + Pointer_stringify(id));
    if (enable) {
        button.disabled = false;
        img.style.opacity = 1.0;
        // following is needed on IE???!!!
        // img.style.filter = 'alpha(opacity:100)';
    } else {
        button.disabled = true;
        img.style.opacity = 0.25;
        // following is needed on IE???!!!
        // img.style.filter = 'alpha(opacity:25)';
    }
},

// -----------------------------------------------------------------------------

jsTickMenuItem: function(id, tick) {
    var menuitem = document.getElementById(Pointer_stringify(id));
    if (tick) {
        menuitem.style.backgroundImage = 'url(images/item_tick.png)';
    } else {
        menuitem.style.backgroundImage = 'url(images/item_blank.png)';
    }
},

// -----------------------------------------------------------------------------

jsSetInputValue: function(id, num) {
    document.getElementById(Pointer_stringify(id)).value = num.toString();
},

// -----------------------------------------------------------------------------

jsGetInputValue: function(id) {
    var num = parseInt(document.getElementById(Pointer_stringify(id)).value, 10);
    if (isNaN(num)) return -1;
    return num;
},

// -----------------------------------------------------------------------------

jsSetCheckBox: function(id, flag) {
    document.getElementById(Pointer_stringify(id)).checked = flag;
},

// -----------------------------------------------------------------------------

jsGetCheckBox: function(id) {
    return document.getElementById(Pointer_stringify(id)).checked;
},

// -----------------------------------------------------------------------------

jsSetInnerHTML: function(id, text) {
    document.getElementById(Pointer_stringify(id)).innerHTML = Pointer_stringify(text);
},

// -----------------------------------------------------------------------------

jsMoveToAnchor: function(anchor) {
    window.location.hash = Pointer_stringify(anchor);
},

// -----------------------------------------------------------------------------

jsSetScrollTop: function(id, pos) {
    document.getElementById(Pointer_stringify(id)).scrollTop = pos;
},

// -----------------------------------------------------------------------------

jsGetScrollTop: function(id) {
    return document.getElementById(Pointer_stringify(id)).scrollTop;
},

// -----------------------------------------------------------------------------

jsBeep: function() {
    var beep = new Audio("beep.wav");
    beep.play();
},

// -----------------------------------------------------------------------------

jsDeleteFile: function(filepath) {
    FS.unlink(Pointer_stringify(filepath));
},

// -----------------------------------------------------------------------------

jsMoveFile: function(inpath, outpath) {
    try {
        FS.rename(Pointer_stringify(inpath), Pointer_stringify(outpath));
        return true;
    } catch (e) {
        alert('FS.rename failed!');
        return false;
    }
},

// -----------------------------------------------------------------------------

jsShowSaveDialog: function(filename, extensions) {
    document.getElementById('save_overlay').style.visibility = 'visible';
    document.getElementById('save_extensions').innerHTML =
        'Valid extensions: ' + Pointer_stringify(extensions);
    var namebox = document.getElementById('save_name');
    namebox.value = Pointer_stringify(filename);
    namebox.select();
    namebox.focus();
},

// -----------------------------------------------------------------------------

jsSaveFile: function(filenameptr) {
    var filename = Pointer_stringify(filenameptr);
    var contents, blob;
    var gzext = '.gz';
    if (filename.length >= gzext.length && filename.substr(filename.length - gzext.length) == gzext) {
        // treat .gz file as uninterpreted binary data
        contents = FS.readFile(filename, {encoding:'binary'});
        blob = new Blob([contents], {type:'application/octet-stream'});
    } else {
        // assume it's a text file (.rle or .mc)
        contents = FS.readFile(filename, {encoding:'utf8'});
        blob = new Blob([contents], {type:'text/plain'});
    }
    var alink = document.createElement('a');
    alink.download = filename;
    alink.innerHTML = 'Download File';
    if (window.webkitURL != null) {
        // Safari/Chrome allows the link to be clicked without actually adding it to the DOM
        alink.href = window.webkitURL.createObjectURL(blob);
    } else {
        // Firefox requires the link to be added to the DOM before it can be clicked
        alink.href = window.URL.createObjectURL(blob);
        alink.onclick = function(event) {
            document.body.removeChild(event.target);
        };
        alink.style.display = 'none';
        document.body.appendChild(alink);
    }
    alink.click();
},

// -----------------------------------------------------------------------------

jsStoreRule: function(rulepath) {
    // read contents of .rule file and save to local storage using rulepath as the key
    var filepath = Pointer_stringify(rulepath);
    try {
        var contents = FS.readFile(filepath, {encoding:'utf8'});
        localStorage.setItem(filepath, contents);
    } catch(e) {
        alert('Failed to store rule file:\n' + filepath);
    }
},

// -----------------------------------------------------------------------------

jsCancelProgress: function() {
    // user hit Cancel button in progress dialog
    GOLLY.cancel_progress = true;
    // best to hide the progress dialog immediately
    document.getElementById('progress_overlay').style.visibility = 'hidden';
},

// -----------------------------------------------------------------------------

jsBeginProgress: function(title) {
    // DEBUG: Module.printErr('jsBeginProgress');
    document.getElementById('progress_title').innerHTML = Pointer_stringify(title);
    document.getElementById('progress_percent').innerHTML = ' ';
    // don't show the progress dialog immediately
    // document.getElementById('progress_overlay').style.visibility = 'visible';
    GOLLY.cancel_progress = false;
    GOLLY.progstart = Date.now()/1000;
},

// -----------------------------------------------------------------------------

jsAbortProgress: function(percentage) {
    // DEBUG: Module.printErr('jsAbortProgress: ' + percentage);
    var secs = Date.now()/1000 - GOLLY.progstart;
    if (document.getElementById('progress_overlay').style.visibility == 'visible') {
        if (percentage < 0) {
            // -ve percentage is # of bytes downloaded so far (file size is unknown)
            percentage *= -1;
            document.getElementById('progress_percent').innerHTML = 'Bytes downloaded: '+percentage;
        } else {
            document.getElementById('progress_percent').innerHTML = 'Completed: '+percentage+'%';
        }
        return GOLLY.cancel_progress;
    } else {
        // note that percentage is not always an accurate estimator for how long
        // the task will take, especially when we use nextcell for cut/copy
        if ( (secs > 1.0 && percentage < 30) || secs > 2.0 ) {
            // task is probably going to take a while so show progress dialog
            document.getElementById('progress_overlay').style.visibility = 'visible';
        }
        return false;
    }
},

// -----------------------------------------------------------------------------

jsEndProgress: function() {
    // DEBUG: Module.printErr('jsEndProgress');
    // hide the progress dialog
    document.getElementById('progress_overlay').style.visibility = 'hidden';
},

// -----------------------------------------------------------------------------

jsDownloadFile: function(urlptr, filepathptr) {
    // download file from given url and store its contents in filepath
    var url = Pointer_stringify(urlptr);
    var filepath = Pointer_stringify(filepathptr);
    // DEBUG: Module.printErr('URL: '+url+' FILE: '+filepath);

    // prefix url with http://www.corsproxy.com/ so we can get file from another domain
    // (note that we assume url starts with "http://")
    url = 'http://www.corsproxy.com/' + url.substring(7);

    var xhr = new XMLHttpRequest();
    if (xhr) {
        // first send a request to get the file's size
        /* doesn't work!!! -- get error 400 (bad request) probably because corsproxy.com
           only supports GET requests
        xhr.onreadystatechange = function() {
            if (xhr.readyState == 4) {
                if (xhr.status == 200) {
                    Module.printErr('response: '+xhr.getResponseHeader('Content-Length'));
                } else {
                    // some sort of error occurred
                    Module.printErr('status error: ' + xhr.status + ' ' + xhr.statusText);
                }
            }
        }
        xhr.open('HEAD', url, true);    // get only the header info
        xhr.send();
        */
        
        // note that we need to prefix jsBeginProgress/jsAbortProgress/jsEndProgress calls
        // with an underscore because webcalls.cpp declares them as extern C routines
        _jsBeginProgress(allocate(intArrayFromString('Downloading file...'), 'i8', ALLOC_STACK));

        xhr.onprogress = function updateProgress(evt) {
            var percent_complete = 0;
            if (evt.lengthComputable) {
                percent_complete = Math.floor((evt.loaded / evt.total) * 100);
                // DEBUG: Module.printErr('Percentage downloaded: ' + percent_complete);
            } else {
                // file size is unknown (this seems to happen for .mc/rle files)
                // so we pass -ve bytes loaded to indicate it's not a percentage
                percent_complete = -evt.loaded;
                // DEBUG: Module.printErr('Bytes downloaded: ' + evt.loaded);
            }
            if (_jsAbortProgress(percent_complete)) {
                // GOLLY.cancel_progress is true
                _jsEndProgress();
                xhr.abort();
            }
        }
        
        xhr.onreadystatechange = function() {
            // DEBUG: Module.printErr('readyState=' + xhr.readyState);
            if (xhr.readyState == 4) {
                _jsEndProgress();
                // DEBUG: Module.printErr('status=' + xhr.status);
                if (xhr.status == 200) {
                    // success, so save binary data to filepath
                    var uInt8Array = new Uint8Array(xhr.response);
                    FS.writeFile(filepath, uInt8Array, {encoding:'binary'});
                    filecreated = Module.cwrap('FileCreated', 'void', ['string']);
                    filecreated(filepath);
                } else if (!GOLLY.cancel_progress) {
                    // some sort of error occurred
                    alert('XMLHttpRequest error: ' + xhr.status);
                }
            }
        }
        
        xhr.open('GET', url, true);
        
        // setting the following responseType will treat all files as binary
        // (note that this is only allowed for async requests)
        xhr.responseType = 'arraybuffer';
        
        xhr.send(null);
    } else {
        alert('XMLHttpRequest failed!');
    }
},

// -----------------------------------------------------------------------------

}; // LibraryGOLLY

autoAddDeps(LibraryGOLLY, '$GOLLY');
mergeInto(LibraryManager.library, LibraryGOLLY);
