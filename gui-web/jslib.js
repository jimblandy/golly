// The following JavaScript routines can be called from C++ code (eg. webcalls.cpp).
// Makefile merges these routines into golly.js via the --js-library option.

mergeInto(LibraryManager.library, {

    jsAlert: function(msg) {
        alert(Pointer_stringify(msg));
    },

    jsConfirm: function(query) {
        return confirm(Pointer_stringify(query));
    },

    jsSetStatusBarColor: function(color) {
        document.getElementById('statusbar').style.backgroundColor = Pointer_stringify(color);
    },

    jsSetAlgo: function(index) {
        document.getElementById('algo').selectedIndex = index;
    },

    jsSetMode: function(index) {
        document.getElementById('mode').selectedIndex = index;
    },

    jsSetState: function(state) {
        document.getElementById('state').innerHTML = state.toString();
    },

    jsSetRule: function(oldrule) {
        var newrule = prompt('Type in a new rule:', Pointer_stringify(oldrule));
        if (newrule == null) {
            return allocate(intArrayFromString('\0'), 'i8', ALLOC_STACK);
        } else {
            return allocate(intArrayFromString(newrule), 'i8', ALLOC_STACK);
        }
    },

    jsShowMenu: function(id, x, y) {
        var menu = document.getElementById(Pointer_stringify(id));
        var mrect = menu.getBoundingClientRect();
        // x,y coords are relative to canvas, so convert to window coords
        var crect = Module['canvas'].getBoundingClientRect();
        // note that scriolling is disabled so window.scrollX and window.scrollY are 0
        x += crect.left + 1;
        y += crect.top + 1;
        // if menu would be outside right/bottom window edge then move it
        if (x + mrect.width > window.innerWidth) x -= mrect.width + 2;
        if (y + mrect.height > window.innerHeight) y -= y + mrect.height - window.innerHeight;
        menu.style.top = y.toString() + 'px';
        menu.style.left = x.toString() + 'px';
        menu.style.visibility = 'visible';
    },

    jsSetClipboard: function(text) {
        document.getElementById('cliptext').value = Pointer_stringify(text);
    },

    jsGetClipboard: function() {
        var text = document.getElementById('cliptext').value;
        if (text == null) {
            return allocate(intArrayFromString('\0'), 'i8', ALLOC_STACK);
        } else {
            return allocate(intArrayFromString(text), 'i8', ALLOC_STACK);
        }
    },

    jsTextAreaIsActive: function() {
        if (document.activeElement.tagName == 'TEXTAREA') {
            return 1;
        } else {
            return 0;
        }
    },

});
