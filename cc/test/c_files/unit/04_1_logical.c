/*d boolean operators: or */
/*@ 0 0 0 0 */
/*@ 0 0 1 1 */
/*@ 0 1 0 1 */
/*@ 0 1 1 1 */

int func(int a, int b, int c) {
    return b || c;
}

