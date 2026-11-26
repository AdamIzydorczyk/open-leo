/* ll_level.c - command-line front-end for ported/ll_level.c (shared viewer).
 * usage: ll_level DATA_DIR LEVEL [-t SECONDS] [--no-audio] [--play]
 *   e.g. ll_level /tmp/opencode/iso_stage/LEO RAN.01
 *   --play upgrades the preview to the playable level prototype (M4-B:
 *   real LEW.MOV physics, .plt ground, arrow-key control; headless CI
 *   driving via LEO_PLAY_SCRIPT + LEO_LEVEL_PPM - see ported/ll_play.h).
 */
#include "ll_level.h"
#include "ll_play.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    /* PR-49: whole lines only. Redirected to a file, stdout is block
     * buffered while stderr is not, so a stderr message could land in the
     * MIDDLE of a stdout line and break the verify legs' greps (it did:
     * one extra report line moved the buffer boundary onto the 'play:
     * LEW.RAN' line). */
    setvbuf(stdout, NULL, _IOLBF, 0);
    /* FIX-13 (X-11): .MOV robustness selftest, no data needed */
    if (argc == 2 && strcmp(argv[1], "--mov-selftest") == 0)
        return ll_mov_selftest() ? 1 : 0;
    /* PR-43 (M4-B): .plt/.MOV-extension/player-sim selftest, hermetic */
    if (argc == 2 && strcmp(argv[1], "--play-selftest") == 0)
        return ll_play_selftest() ? 1 : 0;
    /* PR-137: viewer camera-law selftest, hermetic */
    if (argc == 2 && strcmp(argv[1], "--follow-selftest") == 0)
        return ll_follow_selftest() ? 1 : 0;
    /* PR-531: life-chain selftest, hermetic */
    if (argc == 2 && strcmp(argv[1], "--lives-selftest") == 0)
        return ll_lives_selftest() ? 1 : 0;
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s DATA_DIR LEVEL [-t SECONDS] [--no-audio] "
                "[--list] [--mov-selftest] [--play-selftest] "
                "[--follow-selftest] [--play]\n",
                argv[0]);
        return 2;
    }
    const char *dir = argv[1], *level = argv[2];
    double secs = 5.0;
    int audio = 1;
    int list_only = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
            secs = atof(argv[++i]);
        else if (strcmp(argv[i], "--no-audio") == 0)
            audio = 0;
        else if (strcmp(argv[i], "--list") == 0)
            list_only = 1;
        else if (strcmp(argv[i], "--play") == 0)
            SDL_setenv("LEO_PLAY", "1", 1);   /* SDL_setenv: portable,
                                                 * mingw has no setenv */
    }
    if (list_only) /* headless: wiring + animation inventory, no SDL */
        return ll_level_anims(dir, level) ? 1 : 0;
    if (SDL_Init(SDL_INIT_VIDEO | (audio ? SDL_INIT_AUDIO : 0)) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    int rc = ll_level_show(dir, level, secs, audio);
    SDL_Quit();
    return rc;
}
