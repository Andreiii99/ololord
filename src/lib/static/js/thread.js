/*ololord global object*/

var lord = lord || {};

/*Variables*/

lord.lastSelectedElement = null;
lord.autoUpdateTimer = null;
lord.blinkTimer = null;
lord.pageVisible = "visible";
lord.isDownloading = false;

lord.addAnchorChangeListener = function(callback) {
    if ("onhashchange" in window) {
        window.onhashchange = function() {
            callback(window.location.hash);
        };
    } else {
        var storedHash = window.location.hash;
        window.setInterval(function() {
            if (window.location.hash != storedHash) {
                storedHash = window.location.hash;
                callback(storedHash);
            }
        }, 500);
    }
};

lord.addVisibilityChangeListener = function(callback) {
    if ("hidden" in document)
        document.addEventListener("visibilitychange", callback);
    else if ((hidden = "mozHidden") in document)
        document.addEventListener("mozvisibilitychange", callback);
    else if ((hidden = "webkitHidden") in document)
        document.addEventListener("webkitvisibilitychange", callback);
    else if ((hidden = "msHidden") in document)
        document.addEventListener("msvisibilitychange", callback);
    else if ("onfocusin" in document) //IE 9 and lower
        document.onfocusin = document.onfocusout = callback;
    else //All others
        window.onpageshow = window.onpagehide = window.onfocus = window.onblur = callback;
    if (document["hidden"] !== undefined) {
        callback({
            "type": document["hidden"] ? "blur" : "focus"
        });
    }
};

lord.anchorChangeListener = function(hash) {
    if (!hash || hash.length < 1)
        return;
    lord.selectPost(hash.substring(1));
};

lord.visibilityChangeListener = function(e) {
    var v = "visible";
    var h = "hidden";
    var eMap = {
        "focus": v,
        "focusin": v,
        "pageshow": v,
        "blur": h,
        "focusout": h,
        "pagehide": h
    };
    e = e || window.event;
    if (e.type in eMap)
        lord.pageVisible = eMap[e.type];
    else
        lord.pageVisible = this["hidden"] ? "hidden" : "visible";
    if ("hidden" == lord.pageVisible)
        return;
    if (!lord.blinkTimer)
        return;
    clearInterval(lord.blinkTimer);
    lord.blinkTimer = null;
    var link = lord.id("favicon");
    var finame = link.href.split("/").pop();
    if ("favicon.ico" != finame)
        link.href = link.href.replace("img/favicon_newmessage.ico", "favicon.ico");
    if (document.title.substring(0, 2) == "* ")
        document.title = document.title.substring(2);
};

lord.blinkFaviconNewMessage = function() {
    var link = lord.id("favicon");
    var finame = link.href.split("/").pop();
    if ("favicon.ico" == finame)
        link.href = link.href.replace("favicon.ico", "img/favicon_newmessage.ico");
    else
        link.href = link.href.replace("img/favicon_newmessage.ico", "favicon.ico");
};

lord.selectPost = function(post) {
    if (!!lord.lastSelectedElement)
        lord.lastSelectedElement.className = lord.lastSelectedElement.className.replace(" selectedPost", "");
    lord.lastSelectedElement = null;
    if (isNaN(+post))
        return;
    lord.lastSelectedElement = lord.id("post" + post);
    if (!!lord.lastSelectedElement)
        lord.lastSelectedElement.className += " selectedPost";
    window.location.href = window.location.href.split("#").shift() + "#" + post;
};

lord.insertPostNumberInternal = function(postNumber, position) {
    var field = lord.id("postFormInputText" + position);
    var value = ">>" + postNumber + "\n";
    if (document.selection) {
        field.focus();
        var sel = document.selection.createRange();
        sel.text = value;
    } else if (field.selectionStart || field.selectionStart == "0") {
        var startPos = field.selectionStart;
        var endPos = field.selectionEnd;
        field.value = field.value.substring(0, startPos) + value + field.value.substring(endPos);
    } else {
        field.value += value;
    }
    return field;
};

lord.insertPostNumber = function(postNumber) {
    var field = lord.insertPostNumberInternal(postNumber, "Top");
    if (!field.offsetParent)
        field = lord.insertPostNumberInternal(postNumber, "Bottom");
    if (field.offsetParent)
        field.focus();
};

lord.updateThread = function(boardName, threadNumber, autoUpdate, extraCallback) {
    if (!boardName || isNaN(+threadNumber))
        return;
    var posts = lord.query(".opPost:not(.temporary), .post:not(.temporary)");
    if (!posts)
        return;
    var lastPostN = posts[posts.length - 1].id.replace("post", "");
    lord.ajaxRequest("get_new_posts", [boardName, +threadNumber, +lastPostN], 7, function(res) {
        if (!res)
            return;
        var txt = lord.text((res.length >= 1) ? "newPostsText" : "noNewPostsText");
        if (res.length >= 1)
            txt += " " + res.length;
        if (!autoUpdate)
            lord.showPopup(txt, {classNames: "noNewPostsPopup"});
        if (res.length < 1)
            return;
        var before = lord.id("afterAllPosts");
        if (!before)
            return;
        for (var i = 0; i < res.length; ++i) {
            var post = lord.createPostNode(res[i], true);
            if (!post)
                continue;
            if (lord.id(post.id))
                continue;
            document.body.insertBefore(post, before);
        }
        if (!lord.blinkTimer && "hidden" == lord.pageVisible) {
            lord.blinkTimer = setInterval(lord.blinkFaviconNewMessage, 500);
            document.title = "* " + document.title;
        }
        if (!!extraCallback)
            extraCallback();
    });
};

lord.setAutoUpdateEnabled = function(cbox) {
    var enabled = !!cbox.checked;
    lord.id("autoUpdate_top").checked = enabled;
    lord.id("autoUpdate_bottom").checked = enabled;
    if (enabled) {
        lord.autoUpdateTimer = setInterval(function() {
            var boardName = lord.text("currentBoardName");
            var threadNumber = lord.text("currentThreadNumber");
            lord.updateThread(boardName, threadNumber, true);
        }, 15000);
    } else {
        if (!!lord.autoUpdateTimer) {
            clearInterval(lord.autoUpdateTimer);
            lord.autoUpdateTimer = null;
        }
    }
    lord.setCookie("auto_update", enabled, {
        "expires": lord.Billion
    });
};

lord.postedInThread = function() {
    if (!lord.formSubmitted)
        return;
    var iframe = lord.id("kostyleeque");
    var iframeDocument = iframe.contentDocument || iframe.contentWindow.document;
    var postNumber = lord.queryOne("#postNumber", iframeDocument);
    var referencedPosts = lord.name("referencedPost", iframeDocument);
    lord.nameOne("submit", lord.formSubmitted).disabled = false;
    if (!!postNumber) {
        lord.formSubmitted.reset();
        var divs = lord.query(".postformFile", lord.formSubmitted);
        for (var i = divs.length - 1; i >= 0; --i)
            lord.removeFile(lord.queryOne("a", divs[i]));
        if (lord.customResetForm)
            lord.customResetForm(lord.formSubmitted);
        lord.formSubmitted = null;
        var boardName = lord.text("currentBoardName");
        var threadNumber = lord.text("currentThreadNumber");
        lord.updateThread(boardName, threadNumber, true, function() {
            lord.selectPost(postNumber.value);
        });
        lord.resetCaptcha();
    } else {
        lord.formSubmitted = null;
        var errmsg = lord.queryOne("#errorMessage", iframeDocument);
        var errdesc = lord.queryOne("#errorDescription", iframeDocument);
        lord.showPopup(errmsg.innerHTML + ": " + errdesc.innerHTML, {type: "critical"});
        lord.resetCaptcha();
    }
};

lord.downloadThread = function() {
    if (lord.isDownloading)
        return;
    var as = lord.query(".postFile > .postFileFile > a");
    if (!as || as.length < 1)
        return;
    var dlButton = lord.nameOne("downloadButton");
    if (!dlButton)
        return;
    lord.isDownloading = true;
    dlButton.className += " disabled";
    var progress = lord.node("progress");
    progress.className = "progressBlocking";
    progress.max = as.length;
    progress.value = 0;
    var cButton = lord.node("a");
    cButton.className = "progressBlocking";
    cButton.href = "javascript:void(0);";
    var cancel = false;
    cButton.onclick = function() {
        cancel = true;
        document.body.removeChild(cButton);
        document.body.removeChild(progress);
    };
    cButton.appendChild(lord.node("text", lord.text("cancelButtonText")));
    document.body.appendChild(progress);
    document.body.appendChild(cButton);
    lord.toCenter(progress, progress.offsetWidth, progress.offsetHeight);
    lord.toCenter(cButton, cButton.offsetWidth, cButton.offsetHeight);
    var zip = new JSZip();
    var last = 0;
    var append = function(i) {
        if (i >= as.length) {
            var content = zip.generate({
                "type": "blob"
            });
            if (!cancel) {
                document.body.removeChild(cButton);
                document.body.removeChild(progress);
            }
            saveAs(content, document.title + ".zip");
            lord.isDownloading = false;
            dlButton.className = dlButton.className.replace(" disabled", "");
            return;
        }
        if (cancel) {
            lord.isDownloading = false;
            dlButton.className = dlButton.className.replace(" disabled", "");
            return;
        }
        var a = as[i];
        last = i;
        JSZipUtils.getBinaryContent(a.href, function (err, data) {
            if (!err) {
                zip.file(a.href.split("/").pop(), data, {
                    "binary": true
                });
            }
            progress.value = +progress.value + 1;
            append(++last);
        });
    };
    append(last);
    if (as.length > 1)
        append(++last);
};

lord.initializeOnLoadThread = function() {
    lord.initializeOnLoadBaseBoard();
    lord.addVisibilityChangeListener(lord.visibilityChangeListener);
    lord.addAnchorChangeListener(lord.anchorChangeListener);
    if (lord.getCookie("auto_update") === "true") {
        var cbox = lord.id("autoUpdate_top");
        cbox.checked = true;
        lord.setAutoUpdateEnabled(cbox);
    }
    var sl = window.location.href.split("#");
    if (sl.length != 2)
        return;
    var post = sl[1];
    if (post.substring(0, 1) === "i") {
        post = post.substring(1);
        if (isNaN(+post))
            return;
        lord.showHidePostForm("Top");
        lord.insertPostNumber(post);
    } else {
        lord.selectPost(post);
    }
};

window.addEventListener("load", function load() {
    window.removeEventListener("load", load, false);
    lord.initializeOnLoadThread();
}, false);
