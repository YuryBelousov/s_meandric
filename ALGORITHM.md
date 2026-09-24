# Enumeration algorithms

## Permutations

For $\pi\in S_n$, form the path $0,\pi(1),\ldots,\pi(n),n+1$. Two edges interlace when their endpoints alternate strictly along the line. Permutation $\pi$ is s-meandric if and only if its interlacement graph is bipartite (see Theorem 1 in the corresponding paper). A coloring places the edges on two pages, with no interlacing edges on the same page.

### Frontier state

Sweep vertices in the order $0,1,\ldots,n+1$. An open edge starts behind the sweep and ends ahead of it. The initial state is one edge on page 0, attached to vertex 0. The state records:

| Field | Meaning |
|---|---|
| `size` | Number of open edges, always odd |
| `page[i]` | Page 0 or 1 of edge $i$ |
| `same_start[i]` | Whether edge $i$ starts at the same vertex as edge $i-1$ |
| `mate[i]` | Other open end of this partial path, or `path_start` for vertex 0 |
| `component[i]` | Component in the interlacement graph seen so far |
| `initial_component_open` | Whether the component of the initial edge still meets the frontier |

Edges are ordered by their left endpoints. Component labels follow first appearance, with 0 reserved for the initial component while it remains open. Equal states have identical continuations, so their multiplicities are added.

### Processing a vertex

Sweep vertices in numerical order. At each interior vertex $v$, generate all allowed transitions as follows.

1. **Choose which edges close.** Since $v$ has degree two, close $c\in\{0,1,2\}$ old edges and open $2-c$ new ones. On each page, only the top edge can close, or the top two if both close on that page. Reject a pair with the same start, which would repeat an edge, or a pair of mates, which would create a cycle.

2. **Update interlacement components.** Each closing edge interlaces every surviving edge with a strictly later start. These edges lie on the opposite page. Merge their interlacement components. Edges sharing an endpoint do not interlace.

3. **Check finished components.** After these merges, a component finishes if none of its edges remains open. For each finished noninitial component, consider its edges closing at $v$: require the latest-started one to lie on page 0; otherwise reject the transition. This retains one of the component’s two complementary colorings. The component containing the initial edge already has its orientation fixed.

4. **Update path connectivity.** The `mate` relation records connections through the partial path, independently of interlacement components. If two edges open, their open ends become mates. If one closes and one opens, the new end inherits the closed end’s mate. If two close, join their former mates. The distinguished endpoint at vertex 0 is recorded as `path_start`.

5. **Assign pages and normalize the state.** One new edge may use either page. Two edges opening together are unordered, so their page choices are `00`, `01`, and `11`. Each new edge starts a separate interlacement component. Relabel components canonically as described above.

If the resulting frontier has one edge, complete the path by attaching it to terminal vertex $v+1$. Add the state’s multiplicity to $g_v$ if the remaining edge belongs to the initial interlacement component or lies on page 0. Discard every one-edge frontier after this check: extending it would produce a direct-sum decomposable permutation.

For maximum requested order $M$, discard states with more than $1+2(M-v)$ open edges after vertex $v$: each remaining interior vertex can reduce the frontier by at most two, and completion requires one edge. Together with the growth bound $1+2v$, this limits the frontier to 25 edges for $M \le 24$.

The unique direct-sum decomposition recovers the total counts: $s_0=1$ and $s_n=\sum_{k=1}^{n}g_k s_{n-k}$ for $n \ge 1$.

### Storage

A key contains one initial-component bit and 12 bits per edge: page and tied start use one bit each; mate and component use five each. Mate 31 denotes the path start. Tables and files are separated by frontier size, so a record needs only $\lceil(1+12m)/8\rceil$ key bytes and an eight-byte multiplicity.

Each destination partition scans the source layer and retains children assigned to it by their key hash. Only partition 0 counts completions. Automatic mode chooses 1--32 partitions from the source-state count. Source files are read in blocks, with independent ranges assigned to workers.

Workers merge children into 64 shared table shards, batching updates under a lock per shard. Tables use byte-packed keys, separate counts, and linear probing at up to 75% occupancy. A zero count marks an empty slot. Table growth is serialized so only one shard holds both old and new allocations at a time. Partition, shard, and table indices use distinct hash bits.

Completed partitions are written to disk. Once the whole destination layer is stored, the source files are removed. Temporary files belong to the current run and use the machine's byte order.

## Tangency polynomials

A profile is the sorted multiset of signed cactus factors in the paper's Theorem 5. A bridge contributes $1+x$; a cycle of length $l$ and parity $e$ contributes

$$E_{l,e}(x)=\sum_{j\equiv e\pmod2}\binom ljx^j.$$

Code `1` denotes a bridge and `2*l+e` a cycle; trailing zeros are unused. Positive loops have code `2`. Their polynomial is one, but they must remain in the profile because they affect later insertions. Factor lengths sum to the permutation order.

### Insertion rule

Start with one bridge. Let an inner seed have order $r$, $b$ bridges, cycle multiset $C$, and $w=\sum_{(l,e)\in C}(l+e)\pmod2$. Select one outer factor, replace it as below, and append $C$. Other factors remain unchanged. The new order is $n+r-1$; all parities in the table are modulo two.

| Selected factor | Replacement |
|---|---|
| Bridge | $b$ bridges |
| Cycle $(l,e)$, $b>0$ | $(l-1+b,e+b+1+w)$ |
| Cycle $(l,e)$, $b=0,l>1$ | $(l-1,e+1+w)$ |
| Loop $(1,e)$, $b=0$ | Remove if $e=1+w$; otherwise the insertion is impossible |

These are oriented geometric insertions: reflect the inner permutation in value order when needed to align its terminal arcs. Let $R$ be the product of the inner cycle factors. The contributions with different and equal terminal colors are respectively

$$P^{\ne}=R E_{b,b+w-1},\qquad P^{=}=R E_{b,b+w},$$

with $E_{0,0}=1$ and $E_{0,1}=0$. Removing a marked edge from an outer cycle gives the two factors $E_{l-1,e}$ and $E_{l-1,1-e}$. Pair these with $P^{\ne}$ and $P^{=}$ and apply parity convolution to obtain the table. Geometrically, the insertion replaces the marked spine edge by the inner spine, identifying the matching terminal vertices. This gives the same rules for the full factor multiset.

### Seeds and completeness

The binary snake seeds are $12$ (two bridges) and $21$ (one odd 2-cycle). A monomial seed $x^k$ at order $r$ has $r-k$ positive loops and $k$ negative loops. Through order 24, the possible monomial pairs are $5\le r\le24$, $3\le k<r$, except $(6,4),(7,3),(8,3),(10,4)$.

Completeness uses the prime-factorization theorem: every singular meander is obtained by insertions of snakes and irreducibles. See Y. Belousov, [*Prime factorization of meanders*](https://arxiv.org/abs/2112.10289), Theorem 3 and Proposition 2. Common intervals identify submeanders, so an irreducible permutation is simple. Disconnected interlacement components would give a proper common interval; therefore an irreducible has connected interlacement graph and a monomial profile. This component-interval correspondence follows from Theorem 2 of Bergeron, Heber, and Stoye, [*Common intervals and sorting by reversals*](https://academic.oup.com/bioinformatics/article/18/suppl_2/S54/190353), applied to the signed inverse permutation. Give value $i$ in the inverse a positive sign when $(\pi(i-1),\pi(i),\pi(i+1))$ has even relative order, and a negative sign otherwise; here $\pi(0)=0$ and $\pi(r+1)=r+1$. These signs prevent overlaps at shared endpoints.

An irreducible has at least three touches and cannot have only touches, giving $3\le k<r$. Binary snakes generate all larger snakes. Thus the seeds cover every prime factor, and the insertion rule generates every realizable profile. Conversely, inserting realizable seeds produces realizable profiles.

At each order, expand all profiles and deduplicate their coefficient vectors. The number of vectors is $N_n$; those with constant coefficient one give $O_n$. The coefficient sum is at most $2^{24}$, so 32-bit coefficients suffice; multiplication uses 64 bits.
