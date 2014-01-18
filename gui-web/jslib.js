// The following JavaScript routines can be called from C++ code (see webcalls.cpp).
// Makefile adds this file via the --js-library option.

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

});
