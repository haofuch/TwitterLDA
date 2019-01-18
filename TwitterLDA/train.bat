@if "%4"=="" goto usage

@set dir=%~d1%~p1%~n1.model
@set prefix=%dir%\%~n1
@set input=%1
@set topic=%2
@set iterate=%3
@set thread=%4

@mkdir %dir%

@echo Make buffer ...
TwitterLDA make-buffer --stopword stopwords.txt --input %input% --buffer %prefix%

@echo Train ...
TwitterLDA train --buffer %prefix% --topic %topic% --iterate %iterate% --thread %thread% --output-param %prefix%.%topic%.%iterate% --hyper-param %prefix%.%topic%.hyper-param.txt

@echo Dump parameters ...
TwitterLDA dump-topic --buffer %prefix% --hyper-param %prefix%.%topic%.hyper-param.txt --input-param %prefix%.%topic%.%iterate% --output %prefix%.%topic%.%iterate%.topic-word.txt

@echo Infer ...
TwitterLDA infer-score --thread %thread% --buffer %prefix% --input %input% --hyper-param %prefix%.%topic%.hyper-param.txt --input-param %prefix%.%topic%.%iterate% --output %prefix%.%topic%.%iterate%.infer.txt
@goto end

:usage
@echo Usage %0 input-file #topic #iteration #thread

:end
