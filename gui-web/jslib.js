// The following JavaScript routines can be called from C++ code (see webcalls.cpp).
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
            return allocate(intArrayFromString("\0"), 'i8', ALLOC_STACK);
        } else {
            return allocate(intArrayFromString(newrule), 'i8', ALLOC_STACK);
        }
    },

    jsShowMenu: function(id, x, y) {
        var menu = document.getElementById(Pointer_stringify(id));
        // x,y coords are relative to canvas, so convert to window coords
        var rect = Module['canvas'].getBoundingClientRect();
        x += window.scrollX + rect.left;
        y += window.scrollY + rect.top;
        menu.style.top = y.toString() + 'px';
        menu.style.left = x.toString() + 'px';
        menu.style.visibility = 'visible';
    },

});
