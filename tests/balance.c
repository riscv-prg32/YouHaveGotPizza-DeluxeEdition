/*
 * Monte Carlo balance study (docs/balance.md).
 *
 * The same autoplayer that drives the title-screen demo plays N independent
 * games of each lesson from a fixed seed sequence, with the lives of a new game.
 * Output: one CSV row per lesson. The study is deterministic: the same source
 * gives the same table on every machine.
 *
 *   balance [games-per-lesson]   > docs/data/balance.csv
 */
#include <stdio.h>
#include <stdlib.h>
#include "pz_core.h"
#include "gen/art.inc"

static pz_game_t g;

int main(int argc, char **argv) {
    int games = argc > 1 ? atoi(argv[1]) : 1000;
    const int limit = 30 * 60 * 5; /* five minutes of play, then give up */
    printf("lesson,pizzas,students,games,win_rate,mean_seconds_win,p90_seconds_win,mean_lives_lost_win,mean_flour_used,mean_fed,mean_score,mean_grade_win\n");
    for (int lesson = 0; lesson < 12; ++lesson) {
        int wins = 0;
        double seconds = 0, lost = 0, flour = 0, fed = 0, score = 0, grade = 0;
        static int times[10000];
        for (int n = 0; n < games; ++n) {
            pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, 2654435761u * (uint32_t)(n + 1) + (uint32_t)lesson, lesson);
            int t = 0;
            while (t++ < limit && g.phase != PH_GAMEOVER && g.phase != PH_CLEAR) pz_tick(&g, pz_bot_input(&g));
            flour += g.flour_used;
            fed += g.fed_count;
            score += g.score;
            if (g.phase == PH_CLEAR) {
                times[wins] = (int)(g.lesson_ticks * 33 / 1000);
                seconds += g.lesson_ticks * 0.033;
                lost += g.lives_lost;
                grade += g.grade;
                ++wins;
            }
        }
        /* Insertion sort is fine at this size, and needs no library. */
        for (int i = 1; i < wins; ++i) {
            int v = times[i], k = i - 1;
            while (k >= 0 && times[k] > v) { times[k + 1] = times[k]; --k; }
            times[k + 1] = v;
        }
        pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, 1, lesson);
        int d = wins ? wins : 1;
        printf("%d,%d,%d,%d,%.3f,%.1f,%d,%.2f,%.2f,%.2f,%.0f,%.1f\n", lesson + 1, g.pizza_count, g.student_count, games,
               (double)wins / games, seconds / d, wins ? times[wins * 9 / 10] : 0, lost / d, flour / games, fed / games,
               score / games, grade / d);
    }
    /* The whole campaign: lives carry over, as in a real game. */
    fprintf(stderr, "campaign: ");
    int reached[32] = {0};
    long total_score = 0;
    for (int n = 0; n < games; ++n) {
        pz_new_game(&g, level_data, level_offsets, LEVEL_COUNT, 40503u * (uint32_t)(n + 1), 0);
        int t = 0;
        while (g.phase != PH_GAMEOVER && g.lesson < 24 && t++ < 30 * 60 * 40) {
            if (g.phase == PH_CLEAR) {
                pz_load_lesson(&g, g.lesson + 1);
                continue;
            }
            pz_tick(&g, pz_bot_input(&g));
        }
        reached[g.lesson]++;
        total_score += (long)g.score;
    }
    int at_least = games;
    for (int l = 0; l < 12; ++l) {
        fprintf(stderr, "%d:%.3f ", l + 1, (double)at_least / games);
        at_least -= reached[l];
    }
    fprintf(stderr, "mean_score:%ld\n", total_score / games);
    return 0;
}
