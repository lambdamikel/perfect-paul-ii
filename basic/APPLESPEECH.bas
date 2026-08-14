10 DR =  - 16192
15  HOME 
18  PRINT "AppleSpeech Speech Synthesizer"
19  PRINT 
20  INPUT T$
30  FOR I = 1 TO  LEN(T$): POKE DR, ASC( MID$ (T$,I,1)): NEXT I
50  POKE DR,13
60  RUN 