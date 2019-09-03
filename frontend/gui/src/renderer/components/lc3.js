ace.define("ace/mode/lc3_highlight_rules",["require","exports","module","ace/lib/oop","ace/mode/text_highlight_rules"], function(acequire, exports, module) {
"use strict";

var oop = acequire("../lib/oop");
var TextHighlightRules = acequire("./text_highlight_rules").TextHighlightRules;

var LC3HighlightRules = function() {

    this.$rules = { start:
       [ { token: 'keyword.control.assembly',
           regex: '\\b(?:add|and|br(n?z?p?)?|halt|jmp|jsr|jsrr|ld|ldi|ldr|lea|not|ret|rti|st|sti|str|trap)\\b',
           caseInsensitive: true },
         { token: 'variable.parameter.register.assembly',
           regex: '\\b(?:R(?:[0-7]))\\b',
           caseInsensitive: true },
         { token: 'constant.character.decimal.assembly',
           regex: '[ |,]#[0-9]+\\b' ,
           caseInsensitive: true },
         { token: 'constant.character.hexadecimal.assembly',
           regex: '\\bx[A-F0-9]+\\b',
           caseInsensitive: true },
         { token: 'string.assembly', regex: /'([^\\']|\\.)*'/ },
         { token: 'string.assembly', regex: /"([^\\"]|\\.)*"/ },
         { token: 'support.function.directive.assembly',
           regex: '\\b(.BLKW|.END|.EXTERNAL|.FILL|.ORIG|.STRINGZ)\\b',
           caseInsensitive: true },
         { token: 'comment.assembly', regex: ';(.*)' } ]
    };

    this.normalizeRules();
};

LC3HighlightRules.metaData = { fileTypes: [ 'asm' ],
      name: 'LC3',
      scopeName: 'source.assembly' };


oop.inherits(LC3HighlightRules, TextHighlightRules);

exports.LC3HighlightRules = LC3HighlightRules;
});

ace.define("ace/mode/folding/coffee",["require","exports","module","ace/lib/oop","ace/mode/folding/fold_mode","ace/range"], function(acequire, exports, module) {
"use strict";

var oop = acequire("../../lib/oop");
var BaseFoldMode = acequire("./fold_mode").FoldMode;
var Range = acequire("../../range").Range;

var FoldMode = exports.FoldMode = function() {};
oop.inherits(FoldMode, BaseFoldMode);

(function() {

    this.getFoldWidgetRange = function(session, foldStyle, row) {
        return;
    };
    this.getFoldWidget = function(session, foldStyle, row) {
        return "";
    };

}).call(FoldMode.prototype);

});

ace.define("ace/mode/lc3",["require","exports","module","ace/lib/oop","ace/mode/text","ace/mode/lc3_highlight_rules","ace/mode/folding/coffee"], function(acequire, exports, module) {
"use strict";

var oop = acequire("../lib/oop");
var TextMode = acequire("./text").Mode;
var LC3HighlightRules = acequire("./lc3_highlight_rules").LC3HighlightRules;
var FoldMode = acequire("./folding/coffee").FoldMode;

var Mode = function() {
    this.HighlightRules = LC3HighlightRules;
    this.foldingRules = new FoldMode();
    this.$behaviour = this.$defaultBehaviour;
};
oop.inherits(Mode, TextMode);

(function() {
    this.lineCommentStart = ";";
    this.$id = "ace/mode/lc3";
}).call(Mode.prototype);

exports.Mode = Mode;
});
