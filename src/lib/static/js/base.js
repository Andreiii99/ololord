/*ololord global object*/

var lord = lord || {};

/*Functions*/

lord.changeLocale = function() {
    var sel = lord.id("localeChangeSelect");
    var ln = sel.options[sel.selectedIndex].value;
    lord.setCookie("locale", ln, {
        "expires": lord.Billion, "path": "/"
    });
    lord.reloadPage();
};

lord.doLogin = function() {
    var pwd = lord.text("loginInput");
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
    var inp = lord.id("loginInput");
    if (inp.type === "password")
        inp.type = "text";
    else if (inp.type === "text")
        inp.type = "password";
};

lord.doSearch = function() {
    var query = lord.text("searchFormInputQuery");
    if ("" === query)
        return;
    var sel = lord.id("searchFormSelectBoard");
    var board = sel.options[sel.selectedIndex].value;
    var prefix = lord.text("sitePathPrefix");
    var href = window.location.href.split("/").shift() + "/" + prefix + "search?query=" + encodeURIComponent(query);
    if ("*" !== board)
        href = href + "&board=" + board;
    window.location.href = href;
};

lord.searchKeyPress = function(e) {
    e = e || window.event;
    if (e.keyCode != 13)
        return;
    lord.doSearch();
};

lord.showSettings = function() {
    var div = lord.id("settingsDialogTemplate").cloneNode(true);
    div.style.display = "";
    lord.addClass(div, "settingsDialog");
    var sel = lord.nameOne("quickReplyActionSelect", div);
    var act = lord.getLocalObject("quickReplyAction", "goto_thread");
    lord.queryOne("[value='" + act + "']", sel).selected = true;
    lord.nameOne("hidePostformRules", div).checked = (lord.getCookie("hidePostformRules") == "true");
    var showNewPosts = lord.nameOne("showNewPosts", div);
    showNewPosts.checked = lord.getLocalObject("showNewPosts", true);
    var showYoutubeVideosTitles = lord.nameOne("showYoutubeVideosTitles", div);
    showYoutubeVideosTitles.checked = lord.getLocalObject("showYoutubeVideosTitles", true);
    var checkFileExistence = lord.nameOne("checkFileExistence", div);
    checkFileExistence.checked = lord.getLocalObject("checkFileExistence", true);
    var showAttachedFilePreview = lord.nameOne("showAttachedFilePreview", div);
    showAttachedFilePreview.checked = lord.getLocalObject("showAttachedFilePreview", true);
    var addToFavoritesOnReply = lord.nameOne("addToFavoritesOnReply", div);
    addToFavoritesOnReply.checked = lord.getLocalObject("addToFavoritesOnReply", false);
    var showLeafButtons = lord.nameOne("showLeafButtons", div);
    showLeafButtons.checked = lord.getLocalObject("showLeafButtons", true);
    var leafThroughImagesOnly = lord.nameOne("leafThroughImagesOnly", div);
    leafThroughImagesOnly.checked = lord.getLocalObject("leafThroughImagesOnly", false);
    var imageZoomSensitivity = lord.nameOne("imageZoomSensitivity", div);
    imageZoomSensitivity.value = lord.getLocalObject("imageZoomSensitivity", 25);
    var autoUpdateThreadsByDefault = lord.nameOne("autoUpdateThreadsByDefault", div);
    var defaultAudioVideoVolume = lord.nameOne("defaultAudioVideoVolume", div);
    defaultAudioVideoVolume.value = lord.getLocalObject("defaultAudioVideoVolume", 100);
    autoUpdateThreadsByDefault.checked = !!lord.getLocalObject("autoUpdateThreadsByDefault", false);
    var autoUpdateInterval = lord.nameOne("autoUpdateInterval", div);
    autoUpdateInterval.value = lord.getLocalObject("autoUpdateInterval", 15);
    var showAutoUpdateTimer = lord.nameOne("showAutoUpdateTimer", div);
    showAutoUpdateTimer.checked = !!lord.getLocalObject("showAutoUpdateTimer", true);
    var showAutoUpdateDesktopNotifications = lord.nameOne("showAutoUpdateDesktopNotifications", div);
    showAutoUpdateDesktopNotifications.checked = !!lord.getLocalObject("showAutoUpdateDesktopNotifications", false);
    var hideTripcodes = lord.nameOne("hideTripcodes", div);
    hideTripcodes.checked = !!lord.getLocalObject("hideTripcodes", false);
    var hideUserNames = lord.nameOne("hideUserNames", div);
    hideUserNames.checked = !!lord.getLocalObject("hideUserNames", false);
    var strikeOutHiddenPostLinks = lord.nameOne("strikeOutHiddenPostLinks", div);
    strikeOutHiddenPostLinks.checked = !!lord.getLocalObject("strikeOutHiddenPostLinks", true);
    var userCssEnabled = lord.nameOne("userCssEnabled", div);
    userCssEnabled.checked = !!lord.getLocalObject("userCssEnabled", false);
    lord.showDialog(lord.text("settingsDialogTitle"), null, div, function() {
        var sel = lord.nameOne("modeChangeSelect", div);
        var md = sel.options[sel.selectedIndex].value;
        lord.setCookie("mode", md, {
            "expires": lord.Billion, "path": "/"
        });
        sel = lord.nameOne("styleChangeSelect", div);
        var sn = sel.options[sel.selectedIndex].value;
        lord.setCookie("style", sn, {
            "expires": lord.Billion, "path": "/"
        });
        sel = lord.nameOne("timeChangeSelect", div);
        var tm = sel.options[sel.selectedIndex].value;
        lord.setCookie("time", tm, {
            "expires": lord.Billion, "path": "/"
        });
        if ("local" == tm) {
            var date = new Date();
            lord.setCookie("timeZoneOffset", -date.getTimezoneOffset(), {
                "expires": lord.Billion, "path": "/"
            });
        }
        sel = lord.nameOne("captchaEngineSelect", div);
        var ce = sel.options[sel.selectedIndex].value;
        lord.setCookie("captchaEngine", ce, {
            "expires": lord.Billion, "path": "/"
        });
        var dd = !!lord.nameOne("draftsByDefault", div).checked;
        lord.setCookie("draftsByDefault", dd, {
            "expires": lord.Billion, "path": "/"
        });
        var hr = !!lord.nameOne("hidePostformRules", div).checked;
        lord.setCookie("hidePostformRules", hr, {
            "expires": lord.Billion, "path": "/"
        });
        var hiddenBoards = [];
        lord.query("input", lord.nameOne("hiddenBoards", div)).forEach(function(inp) {
            if (!!inp.checked)
                hiddenBoards.push(inp.name.replace("board_", ""));
        });
        lord.setCookie("hiddenBoards", hiddenBoards.join("|"), {
            "expires": lord.Billion, "path": "/"
        });
        sel = lord.nameOne("quickReplyActionSelect", div);
        var act = sel.options[sel.selectedIndex].value;
        lord.setLocalObject("quickReplyAction", act);
        lord.setLocalObject("showNewPosts", !!showNewPosts.checked);
        lord.setLocalObject("showYoutubeVideosTitles", !!showYoutubeVideosTitles.checked);
        lord.setLocalObject("checkFileExistence", !!checkFileExistence.checked);
        lord.setLocalObject("showAttachedFilePreview", !!showAttachedFilePreview.checked);
        lord.setLocalObject("addToFavoritesOnReply", !!addToFavoritesOnReply.checked);
        lord.setLocalObject("showLeafButtons", !!showLeafButtons.checked);
        lord.setLocalObject("leafThroughImagesOnly", !!leafThroughImagesOnly.checked);
        lord.setLocalObject("imageZoomSensitivity", +imageZoomSensitivity.value);
        lord.setLocalObject("defaultAudioVideoVolume", +defaultAudioVideoVolume.value);
        lord.setLocalObject("autoUpdateThreadsByDefault", !!autoUpdateThreadsByDefault.checked);
        lord.setLocalObject("autoUpdateInterval", +autoUpdateInterval.value);
        lord.setLocalObject("showAutoUpdateTimer", !!showAutoUpdateTimer.checked);
        lord.setLocalObject("showAutoUpdateDesktopNotifications", !!showAutoUpdateDesktopNotifications.checked);
        lord.setLocalObject("hideTripcodes", !!hideTripcodes.checked);
        lord.setLocalObject("hideUserNames", !!hideUserNames.checked);
        lord.setLocalObject("strikeOutHiddenPostLinks", !!strikeOutHiddenPostLinks.checked);
        lord.setLocalObject("userCssEnabled", !!userCssEnabled.checked);
        lord.reloadPage();
    });
};

lord.showFavorites = function() {
    var div = lord.id("favorites");
    if (div)
        return;
    div = lord.node("div");
    div.id = "favorites";
    lord.addClass(div, "favorites");
    var h = lord.node("h1");
    h.appendChild(lord.node("text", lord.text("favoriteThreadsText")));
    div.appendChild(h);
    var fav = lord.getLocalObject("favoriteThreads", {});
    var span = lord.node("span");
    var clBtn = lord.node("button");
    clBtn.appendChild(lord.node("text", lord.text("closeButtonText")));
    clBtn.onclick = function() {
        fav = lord.getLocalObject("favoriteThreads", {});
        lord.forIn(fav, function(o, x) {
            o.previousLastPostNumber = o.lastPostNumber;
            fav[x] = o;
        });
        lord.setLocalObject("favoriteThreads", fav);
        document.body.removeChild(div);
    };
    span.appendChild(clBtn);
    div.appendChild(span);
    var sitePathPrefix = lord.text("sitePathPrefix");
    var f = function(res, x) {
        var postDiv = lord.node("div");
        postDiv.id = "favorite/" + x;
        var a = lord.node("a");
        var boardName = x.split("/").shift();
        var threadNumber = x.split("/").pop();
        a.href = "/" + sitePathPrefix + boardName + "/thread/" + threadNumber + ".html";
        var txt = (res["subject"] ? res["subject"] : res["text"]).substring(0, 150);
        a.appendChild(lord.node("text", "[" + x + "] " + txt.substring(0, 50)));
        a.title = txt;
        a.target = "_blank";
        postDiv.appendChild(a);
        postDiv.appendChild(lord.node("text", " "));
        var fnt = lord.node("font");
        fnt.color = "green";
        postDiv.appendChild(fnt);
        div.insertBefore(postDiv, span);
        var p = fav[x];
        if (p["lastPostNumber"] > p["previousLastPostNumber"]) {
            fnt.appendChild(lord.node("text", "+" + (p["lastPostNumber"] - p["previousLastPostNumber"])));
        }
        var rmBtn = lord.node("a");
        rmBtn.onclick = function() {
            postDiv.parentNode.removeChild(postDiv);
            lord.removeThreadFromFavorites(x.split("/")[0], x.split("/")[1]);
        };
        rmBtn.title = lord.text("removeFromFavoritesText");
        var img = lord.node("img");
        img.src = "/" + sitePathPrefix + "img/delete.png";
        rmBtn.appendChild(img);
        postDiv.appendChild(rmBtn);
    };
    lord.forIn(fav, function(_, x) {
        var boardName = x.split("/").shift();
        var threadNumber = x.split("/").pop();
        lord.ajaxRequest("get_post", [boardName, +threadNumber], lord.RpcGetPostId, function(res) {
            f(res, x);
        });
    });
    document.body.appendChild(div);
    lord.toCenter(div);
};

lord.removeThreadFromFavorites = function(boardName, threadNumber) {
    threadNumber = +threadNumber;
    if (!boardName || isNaN(threadNumber))
        return false;
    var fav = lord.getLocalObject("favoriteThreads", {});
    delete fav[boardName + "/" + threadNumber];
    lord.setLocalObject("favoriteThreads", fav);
    var opPost = lord.id("post" + threadNumber);
    if (!opPost)
        return false;
    var btn = lord.nameOne("addToFavoritesButton", opPost);
    var img = lord.queryOne("img", btn);
    img.src = img.src.replace("favorite_active.png", "favorite.png");
    img.title = lord.text("addThreadToFavoritesText");
    btn.onclick = lord.addThreadToFavorites.bind(lord, boardName, threadNumber);
    return false;
};

lord.checkFavoriteThreads = function() {
    var fav = lord.getLocalObject("favoriteThreads", {});
    var nfav = {};
    lord.forIn(fav, function(o, x) {
        var boardName = x.split("/").shift();
        var threadNumber = x.split("/").pop();
        lord.ajaxRequest("get_new_posts", [boardName, +threadNumber, o.lastPostNumber], lord.RpcGetNewPostsId, function(res) {
            if (!res || res.length < 1)
                return;
            o.lastPostNumber = res[res.length - 1]["number"];
            nfav[x] = o;
        });
    });
    if (lord.notificationsEnabled() && lord.hasOwnProperties(nfav)) {
        var title = lord.text("favoriteThreadsText");
        var sitePathPrefix = lord.text("sitePathPrefix");
        var icon = "/" + sitePathPrefix + "favicon.ico";
        var text = "";
        lord.forIn(nfav, function(v, k) {
            text += k + ", ";
        });
        text = text.substr(0, text.length - 2);
        lord.showNotification(title, text.substr(0, 300), icon);
    }
    setTimeout(function() {
        fav = lord.getLocalObject("favoriteThreads", {});
        lord.forIn(nfav, function(o, x) {
            fav[x].lastPostNumber = o.lastPostNumber;
        });
        lord.setLocalObject("favoriteThreads", fav);
        var div = lord.id("favorites");
        if (div) {
            lord.forIn(fav, function(o, x) {
                var postDiv = lord.id("favorite/" + x);
                var fnt = lord.queryOne("font", postDiv);
                if (fnt.childNodes.length > 0)
                    fnt.removeChild(fnt.childNodes[0]);
                if (o.lastPostNumber > o.previousLastPostNumber)
                    fnt.appendChild(lord.node("text", "+" + (o.lastPostNumber - o.previousLastPostNumber)));
            });
        } else {
            var threadNumber = lord.text("currentThreadNumber");
            lord.forIn(fav, function(o, x) {
                if (o.lastPostNumber > o.previousLastPostNumber) {
                    if (threadNumber && x.split("/").pop() == threadNumber)
                        return;
                    lord.showFavorites();
                }
            });
        }
        setTimeout(lord.checkFavoriteThreads, 15 * lord.Second);
    }, 5 * lord.Second);
};

lord.showNewPosts = function() {
    var lastPostNumbers = lord.getLocalObject("lastPostNumbers", {});
    var currentBoardName = lord.text("currentBoardName");
    var numbers = {};
    var navbar = lord.query(".navbar").shift();
    lord.query(".navbarItemBoard", navbar).forEach(function(item) {
        var a = lord.queryOne("a", item);
        var boardName = a.childNodes[0].nodeValue;
        if (currentBoardName == boardName)
            return;
        numbers[boardName] = +lastPostNumbers[boardName];
        if (isNaN(numbers[boardName]))
            numbers[boardName] = 0;
    });
    lord.ajaxRequest("get_new_post_count_ex", [numbers], lord.RpcGetNewPostCountExId, function(res) {
        if (!res)
            return;
        lord.query(".navbar").forEach(function(navbar) {
            lord.query(".navbarItemBoard", navbar).forEach(function(item) {
                var a = lord.queryOne("a", item);
                var boardName = a.childNodes[0].nodeValue;
                var npc = res[boardName];
                if (!npc)
                    return;
                var span = lord.node("span");
                lord.addClass(span, "newPostCount");
                span.appendChild(lord.node("text", "+" + npc));
                var parent = a.parentNode;
                parent.insertBefore(span, a);
                parent.insertBefore(lord.node("text", " "), a);
            });
        });
    });
};

lord.editUserCss = function() {
    var ta = lord.node("textarea");
    ta.rows = 10;
    ta.cols = 43;
    ta.value = lord.getLocalObject("userCss", "");
    lord.showDialog(null, null, ta, function() {
        lord.setLocalObject("userCss", ta.value);
    });
};

lord.initializeOnLoadSettings = function() {
    if (lord.getCookie("show_tripcode") === "true")
        lord.id("showTripcodeCheckbox").checked = true;
    if (lord.getLocalObject("showNewPosts", true))
        lord.showNewPosts();
    if (lord.getLocalObject("userCssEnabled", false)) {
        var css = lord.getLocalObject("userCss", "");
        var head = lord.queryOne("head");
        var style = lord.node("style");
        style.type = "text/css";
        if (style.styleSheet)
            style.styleSheet.cssText = css;
        else
            style.appendChild(lord.node("text", css));
        head.appendChild(style);
    }
};

window.addEventListener("load", function load() {
    window.removeEventListener("load", load, false);
    lord.initializeOnLoadSettings();
    lord.checkFavoriteThreads();
}, false);

window.addEventListener("beforeunload", function unload() {
    window.removeEventListener("beforeunload", unload, false);
    lord.unloading = true;
}, false);
