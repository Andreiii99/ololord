/*ololord global object*/

var lord = lord || {};

/*Constants*/

lord.Billion = 2 * 1000 * 1000 * 1000;

/*Variables*/

lord.searchFormVisible = {
    "top": false,
    "bottom": false
};

/*Functions*/

lord.showHideSearchForm = function(position) {
    var theButton = document.getElementById("showHideSearchFormButton" + position);
    if (lord.searchFormVisible[position]) {
        theButton.innerHTML = document.getElementById("showSearchFormText").value;
        document.getElementById("searchForm" + position).className = "searchFormInvisible";
    } else {
        theButton.innerHTML = document.getElementById("hideSearchFormText").value;
        document.getElementById("searchForm" + position).className = "searchFormVisible";
    }
    lord.searchFormVisible[position] = !lord.searchFormVisible[position];
};

lord.getCookie = function(name) {
    var matches = document.cookie.match(
        new RegExp("(?:^|; )" + name.replace(/([\.$?*|{}\(\)\[\]\\\/\+^])/g, '\\$1') + "=([^;]*)"));
    return matches ? decodeURIComponent(matches[1]) : undefined;
};

lord.setCookie = function(name, value, options) {
    options = options || {};
    var expires = options.expires;
    if (typeof expires == "number" && expires) {
        var d = new Date();
        d.setTime(d.getTime() + expires * 1000);
        expires = options.expires = d;
    }
    if (expires && expires.toUTCString)
        options.expires = expires.toUTCString();
    value = encodeURIComponent(value);
    var updatedCookie = name + "=" + value;
    for(var propName in options) {
        updatedCookie += "; " + propName;
        var propValue = options[propName];
        if (propValue !== true)
            updatedCookie += "=" + propValue;
    }
    document.cookie = updatedCookie;
};

lord.deleteCookie = function(name) {
    lord.setCookie(name, "", {expires: -1});
};

lord.reloadPage = function() {
    document.location.reload(true);
};

lord.showDialog = function(title, label, body, callback, afterShow) {
    var root = document.createElement("div");
    if (!!title && !!label) {
        var div = document.createElement("div");
        if (!!title) {
            var c = document.createElement("center");
            var t = document.createElement("b");
            t.appendChild(document.createTextNode(title));
            c.appendChild(t);
            div.appendChild(c);
            div.appendChild(document.createElement("br"));
        }
        if (!!label) {
            div.appendChild(document.createTextNode(label));
            div.appendChild(document.createElement("br"));
        }
        root.appendChild(div);
        root.appendChild(document.createElement("br"));
    }
    if (!!body)
        root.appendChild(body);
    var div2 = document.createElement("div");
    var dialog = null;
    var cancel = document.createElement("button");
    cancel.onclick = function() {
        dialog.close();
    };
    cancel.innerHTML = document.getElementById("cancelButtonText").value;
    div2.appendChild(cancel);
    var ok = document.createElement("button");
    ok.onclick = function() {
        if (!!callback)
            callback();
        dialog.close();
    };
    ok.innerHTML = document.getElementById("confirmButtonText").value;
    div2.appendChild(ok);
    root.appendChild(div2);
    dialog = picoModal({
        "content": root
    }).afterShow(function(modal) {
        if (!!afterShow)
            afterShow();
    }).afterClose(function(modal) {
        modal.destroy();
    });
    dialog.show();
};

lord.changeLocale = function() {
    var sel = document.getElementById("localeChangeSelect");
    var ln = sel.options[sel.selectedIndex].value;
    lord.setCookie("locale", ln, {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.changeStyle = function() {
    var sel = document.getElementById("styleChangeSelect");
    var sn = sel.options[sel.selectedIndex].value;
    lord.setCookie("style", sn, {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.changeTime = function() {
    var sel = document.getElementById("timeChangeSelect");
    var ln = sel.options[sel.selectedIndex].value;
    lord.setCookie("time", ln, {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.isHashpass = function(s) {
    return !!s.match(/([0-9a-fA-F]{8}\-){4}[0-9a-fA-F]{8}/g);
};

lord.toHashpass = function(s) {
    if (!s)
        return "";
    var hash = CryptoJS.SHA1(s).toString(CryptoJS.enc.Hex);
    var parts = hash.match(/.{1,8}/g);
    return parts.join("-");
};

lord.doLogin = function() {
    var pwd = document.getElementById("loginInput").value;
    hashpass = lord.isHashpass(pwd) ? pwd : lord.toHashpass(pwd);
    lord.setCookie("hashpass", hashpass, {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.doLogout = function() {
    lord.setCookie("hashpass", "", {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.switchShowLogin = function() {
    var inp = document.getElementById("loginInput");
    if (inp.type === "password")
        inp.type = "text";
    else if (inp.type === "text")
        inp.type = "password";
};

lord.switchShowTripcode = function() {
    var sw = document.getElementById("showTripcodeCheckbox");
    if (!!sw.checked) {
        lord.setCookie("show_tripcode", "true", {
            "expires": lord.Billion, "path": "/"
        });
    } else {
        lord.setCookie("show_tripcode", "", {
            "expires": lord.Billion, "path": "/"
        });
    }
};

lord.initializeOnLoadSettings = function() {
    if (lord.getCookie("show_tripcode") === "true")
        document.getElementById("showTripcodeCheckbox").checked = true;
};
