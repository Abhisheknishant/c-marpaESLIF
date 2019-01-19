#
# This file is adapted from MarpaX::ESLIF::ECMA404
#
package MyRecognizerInterface;
use strict;
use diagnostics;

sub new                    { my ($pkg, $string) = @_; bless { string => $string }, $pkg }
sub read                   { 1 }
sub isEof                  { 1 }
sub isCharacterStream      { 1 }
sub encoding               { }
sub data                   { $_[0]->{string} }
sub isWithDisableThreshold { 0 }
sub isWithExhaustion       { 0 }
sub isWithNewline          { 1 }
sub isWithTrack            { 1 }

package MyValueInterface;
use strict;
use diagnostics;

sub new                { my ($pkg) = @_; bless { result => undef }, $pkg }
sub isWithHighRankOnly { 1 }
sub isWithOrderByRank  { 1 }
sub isWithAmbiguous    { 0 }
sub isWithNull         { 0 }
sub maxParses          { 0 }
sub getResult          { $_[0]->{result} }
sub setResult          { $_[0]->{result} = $_[1] }

package main;
use strict;
use warnings FATAL => 'all';
use Test::More;
use Test::More::UTF8;
use Log::Log4perl qw/:easy/;
use Log::Any::Adapter;
use Log::Any qw/$log/;

#
# Init log
#
our $defaultLog4perlConf = '
log4perl.rootLogger              = INFO, Screen
log4perl.appender.Screen         = Log::Log4perl::Appender::Screen
log4perl.appender.Screen.stderr  = 0
log4perl.appender.Screen.layout  = PatternLayout
log4perl.appender.Screen.layout.ConversionPattern = %d %-5p %6P %m{chomp}%n
';
Log::Log4perl::init(\$defaultLog4perlConf);
Log::Any::Adapter->set('Log4perl');

BEGIN { require_ok('MarpaX::ESLIF') };

my $DATA = do { local $/; <DATA>; };

my @inputs = (
    "{\"test\":[1,2,3]}",
    "{\"test\":\"1\"}",
    "{\"test\":true}",
    "{\"test\":false}",
    "{\"test\":null}",
    "{\"test\":null, \"test2\":\"hello world\"}",
    "{\"test\":\"1.25\"}",
    "{\"test\":\"1.25e4\"}",
    "[]",
    "[
       { 
          \"precision\": \"zip\",
          \"Latitude\":  37.7668,
          \"Longitude\": -122.3959,
          \"Address\":   \"\",
          \"City\":      \"SAN FRANCISCO\",
          \"State\":     \"CA\",
          \"Zip\":       \"94107\",
          \"Country\":   \"US\"
       },
       {
          \"precision\": \"zip\",
          \"Latitude\":  37.371991,
          \"Longitude\": -122.026020,
          \"Address\":   \"\",
          \"City\":      \"SUNNYVALE\",
          \"State\":     \"CA\",
          \"Zip\":       \"94085\",
          \"Country\":   \"US\"
       }
     ]",
    "{
       \"Image\": {
         \"Width\":  800,
         \"Height\": 600,
         \"Title\":  \"View from 15th Floor\",
         \"Thumbnail\": {
             \"Url\":    \"http://www.example.com/image/481989943\",
             \"Height\": 125,
             \"Width\":  \"100\"
         },
         \"IDs\": [116, 943, 234, 38793]
       }
     }",
    "{
       \"source\" : \"<a href=\\\"http://janetter.net/\\\" rel=\\\"nofollow\\\">Janetter</a>\",
       \"entities\" : {
           \"user_mentions\" : [ {
                   \"name\" : \"James Governor\",
                   \"screen_name\" : \"moankchips\",
                   \"indices\" : [ 0, 10 ],
                   \"id_str\" : \"61233\",
                   \"id\" : 61233
               } ],
           \"media\" : [ ],
           \"hashtags\" : [ ],
          \"urls\" : [ ]
       },
       \"in_reply_to_status_id_str\" : \"281400879465238529\",
       \"geo\" : {
       },
       \"id_str\" : \"281405942321532929\",
       \"in_reply_to_user_id\" : 61233,
       \"text\" : \"\@monkchips Ouch. Some regrets are harsher than others.\",
       \"id\" : 281405942321532929,
       \"in_reply_to_status_id\" : 281400879465238529,
       \"created_at\" : \"Wed Dec 19 14:29:39 +0000 2012\",
       \"in_reply_to_screen_name\" : \"monkchips\",
       \"in_reply_to_user_id_str\" : \"61233\",
       \"user\" : {
           \"name\" : \"Sarah Bourne\",
           \"screen_name\" : \"sarahebourne\",
           \"protected\" : false,
           \"id_str\" : \"16010789\",
           \"profile_image_url_https\" : \"https://si0.twimg.com/profile_images/638441870/Snapshot-of-sb_normal.jpg\",
           \"id\" : 16010789,
          \"verified\" : false
       }
     }"
    );

my $eslif = MarpaX::ESLIF->new($log);
isa_ok($eslif, 'MarpaX::ESLIF');

$log->info('Creating JSON grammar');
my $GRAMMAR = MarpaX::ESLIF::Grammar->new($eslif, $DATA);

foreach (0..$#inputs) {
    my $recognizerInterface = MyRecognizerInterface->new($inputs[$_]);
    my $marpaESLIFRecognizerJson = MarpaX::ESLIF::Recognizer->new($GRAMMAR, $recognizerInterface);
    if (! doparse($marpaESLIFRecognizerJson, $inputs[$_], 0)) {
        BAIL_OUT("Failure when parsing:\n$inputs[$_]\n");
    }
}

my $newFromOrshared = 0;
sub doparse {
    my ($marpaESLIFRecognizer, $inputs, $recursionLevel) = @_;
    my $rc;

    if (defined($inputs)) {
        $log->infof('[%d] Scanning JSON', $recursionLevel);
        $log->info ('-------------');
        $log->infof('%s', $inputs);
        $log->info ('-------------');
    } else {
        $log->infof("[%d] Scanning JSON's object", $recursionLevel);
    }
    my $ok = $marpaESLIFRecognizer->scan(1); # Initial events
    while ($ok && $marpaESLIFRecognizer->isCanContinue()) {
        #
        # Resume
        #
        $ok = $marpaESLIFRecognizer->resume();
    }
    if (! $ok) {
        BAIL_OUT("Failure when parsing:\n$inputs\n");
    }
    my $valueInterface = MyValueInterface->new();
    my $status = MarpaX::ESLIF::Value->new($marpaESLIFRecognizer, $valueInterface)->value();
    if (! $status) {
        BAIL_OUT("Failure when valuating:\n$inputs\n");
    }

    my $value = $valueInterface->getResult(); use Data::Dumper; print "RESULT: " . Dumper($value);

    $rc = 1;
    goto done;

  err:
    $rc = 0;

  done:
    return $rc;
}

done_testing();

__DATA__

#
# Default action is to propagate the first RHS value
#
:default ::= action => ::shift

                   #######################################################
                   # >>>>>>>>>>>>>>>> Strict JSON Grammar <<<<<<<<<<<<<<<<
                   #######################################################

# -----------------------------------------
# Start is a value that we want stringified
# -----------------------------------------
:start ::= value2string
value2string ::= value action => ::json

# -------------------
# Composite separator
# -------------------
comma    ::= ','                                  action         => ::undef   # No-op anyway, override ::shift (default action)

# ----------
# JSON value
# ----------
value    ::= string                                                           # ::shift (default action)
           | number                                                           # ::shift (default action)
           | object                                                           # ::shift (default action)
           | array                                                            # ::shift (default action)
           | 'true'                               action => ::true            # built-in true action
           | 'false'                              action => ::false           # built-in false action
           | 'null'                               action => ::lua->lua_null   # built-in undef action

# -----------
# JSON object
# -----------
object   ::= (-'{'-) members (-'}'-)                                          # ::shift (default action)
members  ::= pairs*                               action         => ::lua->lua_members   # Returns { @{pairs1}, ..., @{pair2} }
                                                  separator      => comma     # ... separated by comma
                                                  proper         => 1         # ... with no trailing separator
                                                  hide-separator => 1         # ... and hide separator in the action
                                                  
pairs    ::= string (-':'-) value                 action         => ::lua->lua_pairs     # Returns [ string, value ]

# -----------
# JSON Arrays
# -----------
array    ::= (-'['-) elements (-']'-)                                         # ::shift (default action)
elements ::= value*                               action => ::row             # Returns [ value1, ..., valuen ]
                                                  separator      => comma     # ... separated by comma
                                                  proper         => 1         # ... with no trailing separator
                                                  hide-separator => 1         # ... and hide separator in the action
                                                  

# ------------
# JSON Numbers
# ------------
number ::= NUMBER                                 action => ::lua->lua_number # Prepare for eventual bignum extension

NUMBER   ~ _INT
         | _INT _FRAC
         | _INT _EXP
         | _INT _FRAC _EXP
_INT     ~ _DIGIT
         | _DIGIT19 _DIGITS
         | '-' _DIGIT
         | '-' _DIGIT19 _DIGITS
_DIGIT   ~ [0-9]
_DIGIT19 ~ [1-9]
_FRAC    ~ '.' _DIGITS
_EXP     ~ _E _DIGITS
_DIGITS  ~ /[0-9]+/
_E       ~ /e[+-]?/i

# -----------
# JSON String
# -----------
string     ::= '"' discardOff chars '"' discardOn action => ::copy[2]
discardOff ::=                                                                        # Nullable rule used to disable discard
discardOn  ::=                                                                        # Nullable rule used to enable discard

event :discard[on]  = nulled discardOn                                                # Implementation of discard disabing using reserved ':discard[on]' keyword
event :discard[off] = nulled discardOff                                               # Implementation of discard enabling using reserved ':discard[off]' keyword

chars   ::= filled                                  action => ::lua->lua_chars
filled  ::= char+                                   action => ::concat                # Returns join('', char1, ..., charn)
chars   ::=                                         action => ::lua->lua_empty_string # Prefering empty string instead of undef
char    ::= /[^"\\\x00-\x1F]+/                                                   # ::shift (default action) - take care PCRE2 [:cntrl:] includes DEL character
          | '\\' '"'                             action => ::copy[1]               # Returns double quote, already ok in data
          | '\\' '\\'                           action => ::copy[1]               # Returns backslash, already ok in data
          | '\\' '/'                              action => ::copy[1]               # Returns slash, already ok in data
          | '\\' 'b'                              action => ::u8"\x{08}"
          | '\\' 'f'                              action => ::u8"\x{0C}"
          | '\\' 'n'                              action => ::u8"\x{0A}"
          | '\\' 'r'                              action => ::u8"\x{0D}"
          | '\\' 't'                              action => ::u8"\x{09}"
          | /(?:\\u[[:xdigit:]]{4})+/             action => ::lua->lua_unicode


# -------------------------
# Unsignificant whitespaces
# -------------------------
:discard ::= /[\x{9}\x{A}\x{D}\x{20}]+/

                   #######################################################
                   # >>>>>>>>>>>>>>>>>> JSON Extensions <<<<<<<<<<<<<<<<<<
                   #######################################################

# --------------------------
# Unlimited commas extension
# --------------------------
# /* Unlimited commas */commas   ::= comma+

# --------------------------
# Perl comment extension
# --------------------------
:discard ::= /(?:(?:#)(?:[^\n]*)(?:\n|\z))/u

# --------------------------
# C++ comment extension
# --------------------------
# /* C++ comment */:discard ::= /(?:(?:(?:\/\/)(?:[^\n]*)(?:\n|\z))|(?:(?:\/\*)(?:(?:[^\*]+|\*(?!\/))*)(?:\*\/)))/

# ----------------
# Number extension
# ----------------
#
# number ::= /\-?(?:(?:[1-9]?[0-9]+)|[0-9])(?:\.[0-9]+)?(?:[eE](?:[+-])?[0-9]+)?/ # /* bignum */action => ::lua->lua_number

# /* nan */number   ::= '-NaN':i                               action => ::lua->lua_nan
# /* nan */number   ::=  'NaN':i                               action => ::lua->lua_nan
# /* nan */number   ::= '+NaN':i                               action => ::lua->lua_nan
# /* inf */number   ::= '-Infinity':i                          action => ::lua->lua_negative_infinity
# /* inf */number   ::=  'Infinity':i                          action => ::lua->lua_positive_infinity
# /* inf */number   ::= '+Infinity':i                          action => ::lua->lua_positive_infinity
# /* inf */number   ::= '-Inf':i                               action => ::lua->lua_negative_infinity
# /* inf            ::=  'Inf':i                               action => ::lua->lua_positive_infinity
# /* inf */number   ::= '+Inf':i                               action => ::lua->lua_positive_infinity

# -----------------
# Control character
# -----------------
# /* cntrl */char      ::= /[\x00-\x1F]/                                                          # Because [:cntrl:] includes DEL (x7F)

# -----------------
# Lua actions      
# -----------------
<luascript>
  function table_print (tt, indent, done)
    done = done or {}
    indent = indent or 0
    if type(tt) == "table" then
      for key, value in pairs (tt) do
        io.write(string.rep (" ", indent)) -- indent it
        if type (value) == "table" and not done [value] then
          done [value] = true
          io.write(string.format("[%s] => table\n", tostring (key)));
          io.write(string.rep (" ", indent+4)) -- indent it
          io.write("(\n");
          table_print (value, indent + 7, done)
          io.write(string.rep (" ", indent+4)) -- indent it
          io.write(")\n");
        else
          io.write(string.format("[%s] => %s\n",
              tostring (key), tostring(value)))
        end
      end
    else
      io.write(tostring(tt) .. "\n")
    end
  end
  io.stdout:setvbuf('no')
  -----------------------------------
  function lua_null()
    -- Special case to have nil persistency: we will return a table saying we want it to be opaque to marpaESLIF:

    -- This table's metatable will host: an opaque flag and the representation.
    -- The opaque flag is a__marpaESLIF_opaque metafield that must be a boolean.
    -- The __tostring standard metafield gives the representation, and must be a function that returns a string.
    local _mt = {}
    _mt.__marpaESLIF_opaque = true
    _mt.__tostring = function() return 'null' end

    -- We COULD have set also a __marpaESLIF_opaque encoding metafield but then, during representation, marpaESLIF
    -- will understand this is a string, thus enclosing the output in double quotes. Without the encoding metafield
    -- marpaESLIF will consider this as an ARRAY of bytes and use it verbatim in the representation. We just have:
    -- to make sure this is UTF-8 compatible in this case.
    -- When set, the __marpaESLIF_encoding metafield must be a function that returns a string:
    -- _mt.__marpaESLIF_encoding = function() return 'UTF-8' end

    local _result = {}
    setmetatable(_result, _mt) 
    return _result
  end
  -----------------------------------
  function lua_members(...)
    local _result = {}
    for _i=1,select('#', ...) do
      local _pair = select(_i, ...)
      _result[_pair[1]] = _pair[2]
    end
    local _mt = {}
    _mt.__marpaESLIF_canarray = false -- hint to say that we never want that to appear as a marpaESLIF array
    setmetatable(_result, _mt) 
    return _result
  end
  -----------------------------------
  function lua_pairs(key, value)
    return { key, value }
  end
  -----------------------------------
  function lua_number(number)
    local _result = tonumber(number)
    return _result
  end
  -----------------------------------
  function lua_empty_string()
    return ''
  end
  -----------------------------------
  function lua_chars(chars)
    -- marpaESLIFStringHelper_new returns an object that these metafields:
    -- __tostring: a function that returns the representation as a string
    -- __marpaESLIF_encoding: a function that returns the encoding as a string
    -- __marpaESLIF_opaque: a boolean that says to remain opaque to marpaESLIF
    return chars, 'UTF-8'
  end
  -----------------------------------
  function lua_unicode(u)
    local _hex = {}
    local _maxpos = string.len(u)
    local _nextArrayIndice = 1
    local _pos = 1

    -- Per def u is a sequence of \\u[[:xdigit:]]{4} i.e. 6 'characters', ahem bytes
    while (_pos < _maxpos) do
       -- Extract the [[:xdigit:]]{4} part
      local _codepointAsString = string.sub(u, _pos + 2, _pos + 5)      local _codepoint = tonumber(_codepointAsString, 16)
      _hex[_nextArrayIndice] = _codepoint
      _nextArrayIndice = _nextArrayIndice + 1
      _pos = _pos + 6
    end

    local _result = ''
    local _high
    local _low
    local _codepoint
    while (#_hex > 0) do
      if (#_hex > 1) then
        _high, _low = table.unpack(_hex, 1, 2)
        -- UTF-16 surrogate pair ?
        if ((_high >= 0xD800) and (_high <= 0xDBFF) and (_low >= 0xDC00) and (_low <= 0xDFFF)) then
          _codepoint = ((_high - 0xD800) * 0x400) + (_low - 0xDC00) + 0x10000
          table.remove(_hex, 1)
          table.remove(_hex, 1)
        else
          _codepoint = _high
          table.remove(_hex, 1)
        end
      else
        _codepoint = table.remove(_hex, 1)
      end
      _result = _result..utf8.char(_codepoint)
    end

      return marpaESLIF:marpaESLIFStringHelper_new(_result, 'UTF-8')
  end
</luascript>
