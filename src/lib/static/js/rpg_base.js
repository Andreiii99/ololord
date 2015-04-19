/*ololord global object*/

var lord = lord || {};

/*Functions*/

lord.customEditFormSet = function(form, post) {
    var variants = lord.nameOne("voteVariants", form);
    var count = lord.nameOne("voteVariantCount", form);
    var createInp = function(id, text) {
        var lastN = +count.value;
        var div = lord.node("div");
        var inp = lord.node("input");
        inp.type = "text";
        inp.name = "voteVariant" + (lastN + 1);
        inp.size = "43";
        div.appendChild(inp);
        var a = lord.node("a");
        a.onclick = (function(inp) {
            var div = inp.parentNode;
            variants.removeChild(div);
            var i = 0;
            count.value = i;
            lord.query("div > input", variants).forEach(function(inp) {
                ++i;
                inp.name = "voteVariant" + i;
                count.value = i;
            });
        }).bind(lord, inp);
        var img = lord.node("img");
        img.src = "/" + lord.text("sitePathPrefix") + "img/delete.png";
        img.title = lord.text("removeVoteVariantText");
        a.appendChild(img);
        div.appendChild(a);
        variants.appendChild(div);
        count.value = (lastN + 1);
        if (id)
            inp.voteId = id;
        if (text)
            inp.value = text;
        return inp;
    };
    variants.nextSibling.nextSibling.onclick = function() {
        createInp();
    };
    if (!lord.nameOne("voteText", post))
        return;
    var text = lord.nameOne("voteText", form);
    var multiple = lord.nameOne("multipleVoteVariants", form);
    text.value = lord.nameOne("voteText", post).innerHTML;
    var vv = lord.nameOne("voteVariants", post);
    multiple.checked = ("true" == lord.queryOne("input[type='hidden']", vv).value);
    count.value = 0;
    lord.query("input:not([type='hidden'])", vv).forEach(function(inp) {
        var id = multiple.checked ? inp.name.replace("voteVariant", "") : inp.value;
        var text = inp.nextSibling.nodeValue
        text = text.substr(1, text.length - 2); //NOTE: Removig the spaces
        createInp(id, text);
    });
};

lord.customEditFormGet = function(form, params) {
    var text = lord.nameOne("voteText", form);
    var multiple = lord.nameOne("multipleVoteVariants", form);
    var variants = lord.nameOne("voteVariants", form);
    var data = {};
    data.text = text.value;
    data.multiple = !!multiple.checked;
    data.variants = [];
    lord.query("div > input", variants).forEach(function(inp) {
        var v = {};
        v.text = inp.value;
        v.id = inp.voteId ? inp.voteId : "";
        data.variants.push(v);
    });
    return data;
};

lord.addVoteVariant = function(pos) {
    parent.appendChild(div);
    lord.id("voteVariantCount" + pos).value = (lastN + 1);
};

lord.createPostNodeCustom = function(post, res, permanent, boardName) {
    var variants = res["voteVariants"];
    var tr = lord.nameOne("voteTr", post);
    if (variants && variants.length) {
        var disabled = !!res["voteDisabled"];
        var voted = !!res["voteVoted"];
        var multiple = !!res["voteMultiple"];
        var div = lord.nameOne("voteVariants", post);
        if (!res["voteDisabled"]) {
            var closed = lord.nameOne("voteClosedImg", post);
            closed.parentNode.removeChild(closed);
        }
        lord.nameOne("voteText", post).appendChild(lord.node("text", (res["disabled"] ? " " : "") + res["voteText"]));
        lord.queryOne("input", div).value = multiple ? "true" : "false";
        variants.forEach(function(v) {
            var inp = lord.node("input");
            if (multiple) {
                inp.type = "checkbox";
                inp.name = "voteVariant" + v.id;
            } else {
                inp.type = "radio";
                inp.name = "voteGroup";
                inp.value = v.id;
            }
            if (!!res["ownPost"] || disabled || voted)
                inp.disabled = "true";
            inp.checked = !!v.selected;
            div.appendChild(inp);
            div.appendChild(lord.node("text", " " + v.text + " "));
            var span = lord.node("span");
            span.className = "voteCount";
            span.appendChild(lord.node("text", "(" + lord.text("votedText") + " " + v.voteCount + ")"));
            div.appendChild(span);
            div.appendChild(lord.node("br"));
        });
        var btnVote = lord.nameOne("buttonVote", tr);
        var btnUnvote = lord.nameOne("buttonUnvote", tr);
        var btnClose = lord.nameOne("buttonSetVoteClosed", tr);
        var btnOpen = lord.nameOne("buttonSetVoteOpened", tr);
        if (!!res["ownPost"]) {
            if (!!res["voteDisabled"]) {
                btnOpen.onclick = lord.setVoteOpened.bind(lord, res["number"], true);
                btnClose.parentNode.removeChild(btnClose);
            } else {
                btnClose.onclick = lord.setVoteOpened.bind(lord, res["number"], false);
                btnOpen.parentNode.removeChild(btnOpen);
            }
            btnVote.parentNode.removeChild(btnVote);
            btnUnvote.parentNode.removeChild(btnUnvote);
        } else {
            if (disabled) {
                btnVote.disabled = "true";
                btnUnvote.disabled = "true";
            } else {
                if (voted) {
                    btnUnvote.onclick = lord.unvote.bind(lord, +res["number"]);
                    btnVote.disabled = "true";
                } else {
                    btnVote.onclick = lord.vote.bind(lord, +res["number"]);
                    btnUnvote.disabled = "true";
                }
            }
            btnClose.parentNode.removeChild(btnClose);
            btnOpen.parentNode.removeChild(btnOpen);
        }
    } else {
        tr.parentNode.removeChild(tr);
    }
};

lord.customResetForm = function(form) {
    var pos = form.id.replace("postForm", "");
    var parent = lord.id("voteVariants" + pos);
    if (!parent)
        return;
    lord.arr(parent.children).forEach(function(el) {
        parent.removeChild(el);
    });
    lord.id("voteVariantCount" + pos).value = 0;
    var text = lord.id("voteText" + pos);
    text.parentNode.replaceChild(text.cloneNode(false), text);
};

lord.addVoteVariant = function(pos) {
    var parent = lord.id("voteVariants" + pos);
    var variants = lord.query("div > input", parent);
    var lastN = (variants && variants.length) ? +lord.last(variants).name.replace("voteVariant", "") : 0;
    var div = lord.node("div");
    var inp = lord.node("input");
    inp.type = "text";
    inp.name = "voteVariant" + (lastN + 1);
    inp.size = "43";
    div.appendChild(inp);
    var a = lord.node("a");
    a.onclick = lord.removeVoteVariant.bind(lord, div);
    var img = lord.node("img");
    img.src = "/" + lord.text("sitePathPrefix") + "img/delete.png";
    img.title = lord.text("removeVoteVariantText");
    a.appendChild(img);
    div.appendChild(a);
    parent.appendChild(div);
    lord.id("voteVariantCount" + pos).value = (lastN + 1);
};

lord.removeVoteVariant = function(div) {
    var parent = div.parentNode;
    parent.removeChild(div);
    var count = lord.nameOne("voteVariantCount", parent.parentNode);
    var i = 0;
    count.value = i;
    lord.query("div > input", div).forEach(function(inp) {
        ++i;
        inp.name = "voteVariant" + i;
        count.value = i;
    });
};

lord.vote = function(postNumber) {
    postNumber = +postNumber;
    if (isNaN(postNumber) || postNumber <= 0)
        return;
    var post = lord.id("post" + postNumber);
    if (!post)
        return;
    var votes = [];
    var variants = lord.nameOne("voteVariants", post);
    var multiple = ("true" == lord.queryOne("input[type='hidden']", variants).value);
    if (multiple) {
        lord.query("input[type='checkbox']").forEach(function(inp) {
            if (!!inp.checked)
                votes.push(inp.name.replace("voteVariant", ""));
        });
    } else {
        lord.query("input[type='radio']").forEach(function(inp) {
            if (!!inp.checked)
                votes.push(inp.value);
        });
    }
    lord.ajaxRequest("vote", [postNumber, votes], 11, function() {
        lord.updatePost("rpg", postNumber, post);
    });
};

lord.unvote = function(postNumber) {
    postNumber = +postNumber;
    if (isNaN(postNumber) || postNumber <= 0)
        return;
    var post = lord.id("post" + postNumber);
    if (!post)
        return;
    lord.ajaxRequest("unvote", [postNumber], 12, function() {
        lord.updatePost("rpg", postNumber, post);
    });
};

lord.setVoteOpened = function(postNumber, opened) {
    postNumber = +postNumber;
    if (isNaN(postNumber) || postNumber <= 0)
        return;
    var post = lord.id("post" + postNumber);
    if (!post)
        return;
    lord.ajaxRequest("set_vote_opened", [postNumber, !!opened], 13, function() {
        lord.updatePost("rpg", postNumber, post);
    });
};
