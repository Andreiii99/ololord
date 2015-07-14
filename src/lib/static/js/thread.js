/*ololord global object*/

var lord = lord || {};

/*Classes*/

/*constructor*/ lord.AutoUpdateTimer = function(intervalSeconds, showCountdown) {
    this.intervalSeconds = intervalSeconds;
    this.showCountdown = showCountdown;
    this.updateTimer = null;
    this.countdownTimer = null;
    this.secondsLeft = 0;
};

/*private*/ lord.AutoUpdateTimer.prototype.createCountdownTimer = function() {
    this.secondsLeft = this.intervalSeconds;
    this.countdownTimer = setInterval((function() {
        this.secondsLeft -= 1;
        if (this.secondsLeft <= 0)
            this.secondsLeft = this.intervalSeconds;
        this.update();
    }).bind(this), lord.Second);
};

/*private*/ lord.AutoUpdateTimer.prototype.update = function() {
    lord.name("autoUpdateText").forEach((function(span) {
        lord.removeChildren(span);
        var txt = lord.text("autoUpdateText");
        if (this.countdownTimer)
            txt += ": " + this.secondsLeft;
        span.appendChild(lord.node("text", txt));
    }).bind(this));
};

/*public*/ lord.AutoUpdateTimer.prototype.start = function() {
    if (this.updateTimer)
        return;
    this.updateTimer = setInterval((function() {
        var boardName = lord.text("currentBoardName");
        var threadNumber = lord.text("currentThreadNumber");
        lord.updateThread(boardName, threadNumber, true);
        if (this.countdownTimer) {
            clearInterval(this.countdownTimer);
            this.createCountdownTimer();
        }
        this.update();
    }).bind(this), this.intervalSeconds * lord.Second);
    if (this.showCountdown)
        this.createCountdownTimer();
    this.update();
};

/*public*/ lord.AutoUpdateTimer.prototype.stop = function() {
    if (!this.updateTimer)
        return;
    clearInterval(this.updateTimer);
    this.updateTimer = null;
    if (this.countdownTimer) {
        clearInterval(this.countdownTimer);
        this.countdownTimer = null;
    }
    this.secondsLeft = 0;
    this.update();
};

/*Variables*/

lord.lastSelectedElement = null;
lord.autoUpdateTimer = null;
lord.blinkTimer = null;
lord.pageVisible = "visible";
lord.isDownloading = false;

/*Functions*/

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
    if (lord.lastSelectedElement)
        lord.removeClass(lord.lastSelectedElement, "selectedPost");
    lord.lastSelectedElement = null;
    if (isNaN(+post))
        return;
    lord.lastSelectedElement = lord.id("post" + post);
    if (lord.lastSelectedElement)
        lord.addClass(lord.lastSelectedElement, "selectedPost");
    window.location.href = window.location.href.split("#").shift() + "#" + post;
};

lord.updateThread = function(boardName, threadNumber, autoUpdate, extraCallback) {
    if (!boardName || isNaN(+threadNumber))
        return;
    var posts = lord.query(".opPost:not(.temporary), .post:not(.temporary)");
    if (!posts)
        return;
    var lastPost = posts[posts.length - 1];
    var lastPostN = lastPost.id.replace("post", "");
    var seqNum = +lord.queryOne(".postSequenceNumber", lastPost).textContent;
    lord.ajaxRequest("get_new_posts", [boardName, +threadNumber, +lastPostN], lord.RpcGetNewPostsId, function(res) {
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
            if (!isNaN(seqNum))
                lord.queryOne(".postSequenceNumber", post).appendChild(lord.node("text", ++seqNum));
            document.body.insertBefore(post, before);
            lord.postNodeInserted(post);
        }
        if ("hidden" == lord.pageVisible) {
            if (!lord.blinkTimer) {
                lord.blinkTimer = setInterval(lord.blinkFaviconNewMessage, 500);
                document.title = "* " + document.title;
            }
            if (lord.notificationsEnabled()) {
                var subject = lord.queryOne(".theTitle > h1").textContent;
                var title = "[" + subject + "] " + lord.text("newPostsText") + " " + res.length;
                var sitePathPrefix = lord.text("sitePathPrefix");
                var icon = "/" + sitePathPrefix + "favicon.ico";
                var p = res[0];
                if (p.files && p.files.length > 0)
                    icon = "/" + sitePathPrefix + lord.text("currentBoardName") + "/" + p.files[0].thumbName;
                lord.showNotification(title, p.rawPostText.substr(0, 300), icon);
            }
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
        var intervalSeconds = lord.getLocalObject("autoUpdateInterval", 15);
        var showCountdown = lord.getLocalObject("showAutoUpdateTimer", true);
        lord.autoUpdateTimer = new lord.AutoUpdateTimer(intervalSeconds, showCountdown);
        lord.autoUpdateTimer.start();
    } else if (lord.autoUpdateTimer) {
        lord.autoUpdateTimer.stop();
        lord.autoUpdateTimer = null;
    }
    var list = lord.getLocalObject("autoUpdate", {});
    var threadNumber = lord.text("currentThreadNumber");
    list[threadNumber] = enabled;
    lord.setLocalObject("autoUpdate", list);
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
    lord.addClass(dlButton, "disabled");
    var progress = lord.node("progress");
    lord.addClass(progress, "progressBlocking");
    progress.max = as.length;
    progress.value = 0;
    var cButton = lord.node("a");
    lord.addClass(cButton, "progressBlocking");
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
    var completed = 0;
    var append = function(i) {
        if (cancel) {
            lord.isDownloading = false;
            lord.removeClass(dlButton, "disabled");
            return;
        }
        var a = as[i];
        JSZipUtils.getBinaryContent(a.href, function (err, data) {
            if (!err) {
                zip.file(a.href.split("/").pop(), data, {
                    "binary": true
                });
            }
            ++completed;
            progress.value = +progress.value + 1;
            if (completed == as.length) {
                var content = zip.generate({
                    "type": "blob"
                });
                if (!cancel) {
                    document.body.removeChild(cButton);
                    document.body.removeChild(progress);
                }
                saveAs(content, document.title + ".zip");
                lord.isDownloading = false;
                lord.removeClass(dlButton, "disabled");
            }
            if (last < as.length - 1)
                append(++last);
        });
    };
    append(last);
    if (as.length > 1)
        append(++last);
};

lord.initializeOnLoadThread = function() {
    lord.addVisibilityChangeListener(lord.visibilityChangeListener);
    lord.addAnchorChangeListener(lord.anchorChangeListener);
    var enabled = lord.getLocalObject("autoUpdate", {})[lord.text("currentThreadNumber")];
    if (true === enabled || (false !== enabled && lord.getLocalObject("autoUpdateThreadsByDefault", false))) {
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
