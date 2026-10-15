# Host build of the Perfect Paul ][ SP0256 firmware's protocol and voice.
# SPDX-License-Identifier: GPL-3.0-or-later
CFLAGS   = -O2 -std=gnu11 -I.. -I../core -I../vendor -Wall -Wextra
CXXFLAGS = -O2 -std=gnu++11 -I../core -I../vendor -Wall

OBJS = pp2_render.o pp2_sp0256.o nrl.o sp0256_speak.o sp0256_rom.o sp0256.o sp0256_al2.o

all: pp2_render nrl_test second_utterance

pp2_render: $(OBJS)
	$(CXX) -o $@ $(OBJS)

NRLOBJS = nrl_test.o nrl.o pp2_sp0256.o sp0256_speak.o sp0256_rom.o sp0256.o sp0256_al2.o
nrl_test: $(NRLOBJS)
	$(CXX) -o $@ $(NRLOBJS)

SECONDOBJS = second_utterance.o pp2_sp0256.o nrl.o sp0256_speak.o sp0256_rom.o sp0256.o sp0256_al2.o
second_utterance: $(SECONDOBJS)
	$(CXX) -o $@ $(SECONDOBJS)

second_utterance.o: second_utterance.c ; $(CC) $(CFLAGS) -c -o $@ $<
nrl_test.o: nrl_test.c ; $(CC) $(CFLAGS) -c -o $@ $<
nrl.o: ../nrl.c ; $(CC) $(CFLAGS) -c -o $@ $<

pp2_render.o: pp2_render.c ; $(CC) $(CFLAGS) -c -o $@ $<
pp2_sp0256.o: ../pp2_sp0256.c ; $(CC) $(CFLAGS) -c -o $@ $<
sp0256_speak.o: ../core/sp0256_speak.c ; $(CC) $(CFLAGS) -c -o $@ $<
sp0256_rom.o: ../core/sp0256_rom.cpp ; $(CXX) $(CXXFLAGS) -c -o $@ $<
sp0256.o: ../vendor/sp0256.c ; $(CC) $(CFLAGS) -c -o $@ $<
sp0256_al2.o: ../vendor/sp0256_al2.cpp ; $(CXX) $(CXXFLAGS) -c -o $@ $<

clean: ; rm -f $(OBJS) $(NRLOBJS) $(SECONDOBJS) pp2_render nrl_test second_utterance
