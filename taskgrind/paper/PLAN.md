0) Abstract

1) Introduction
    - Paralléliser des applications par tâche dépendentes = fastidieux
    - Aucunes garantis sur la justesse des codes

2) Motivations
    2.1) How programmers are currently debugging their task-based parallel applications
        - Helgrind, ThreadSanitizer (<=> Archer)
    2.2) Proposed Analysis
        - E.1. - Tasks A may write 'x' while B access it
        - E.2. - Tasks A and B cannot run concurrently despite having no data dependency

    - Program gives expressed tasks and their dependencies
    - Present Taskgrind interfaces here (~4-5 interfaces)

3) Taskgrind Design
    - Taskgrind associate memory accesses (read, write) to tasks
        - Stored in a "sparse memory tree" to construct intervals, and perform union/intersect between tasks
    - Perform analysis at the end of execution with an entire view of the program
        - with respect on (RaW, WaR, WaW) data dependences following the sequential order of execution

4) Briding OMPT with Taskgrind

5) Footprint on memory and execution time

6) Evaluation (false positive, false negative)
    5.1) Define a set of benchmark (it should be easy, many OpenMP programs out-there)
    5.2) Measure false-positive, (false-negative shouldn't be any!! as opposed to ThreadSanitizer)
    5.3) Measurement on slowdown

7) Related Works
    - Automatic Parallelization [12, 13]

8) Conclusion

# BIBLIO

[1] « Compiler Automatic Discovery of OmpSs Task Dependencies » - S. Royuela - https://link.springer.com/chapter/10.1007/978-3-642-37658-0_16
-> compilation-only, donc limité

[2] « Detecting Non-Sibling Dependencies in OpenMP Task-Based Application » - Patrick - https://hal.science/hal-02177469/document
-> OMPT only, donc pas moyen de créer le graphe (b)

[3] Eric Aubanel et Leah Bidlake: leur biblio sur la representation mental du parallelisme morive le besoin d'un outil

[4] A Toolchain to Verify the Parallelization of OmpSs-2 Applications - 2020
Ils check les "erreurs" suivantes
- E1 - accès dans la tâche, non spécifié dans le constructeur
- E2 - l'inverse de E1
- E3 - detection de sibling dependencies similaire à [2]
- E4 - l'inverse de E3
- E5 - propose l'ajout de taskwait pour garantir RaW selon les constructeurs de tâches filles

Une différence fondamentale: E1 est LOCAL !!
Si la donnée n'est accédé par aucunes autres tâches, alors ce n'est pas un problème de ne pas mettre de dépendances. et l'ordre d'exécution est peut être déjà garanti par une autre dépendances

Notre positionnement par rapport à eux ce serait:
- on incorpore ça dans taskgrind
- taskgrind permet aussi de faire d'autres outils d'analyse (carthographie des accès mémoires, arcs redondants...)

[5] Starsscheck: A tool to find errors in task-based parallel programs - 2010
Vérifie que la tâche n'accède qu'aux données déclarées en input/output
Problème: dépendances != accès réel, exemple ceci peut être correct:

    out(x)
        x = 0;
        y = 0;

    in(x)
        f(x, y)

[6] Helgrind - https://valgrind.org/docs/manual/hg-manual.html
- Conçu pour race condition thread POSIX

[7] Does It Matter? - OMPSanitizer: An Impact Analyzer of Reported Data Races in OpenMP Programs

[8] Valgrind et shadow memory
- on n'a pas beosin de ça, on veut des intervales

[9] Implementing OmpSs Support for Regions of Data in Architectures with Multiple Address Spaces
- matching sur des régions (<=> intersection non nul d'un ensemble de segments)

[10] ThreadSanitizer – data race detection in practice (2009)
- Skip des regions par analyse statique à la compil
- Analyse dynamique, compilo injecte des instructions (pas de VM, execution native)

[11] ARCHER: Effectively Spotting Data Races in Large OpenMP Applications
    - Detecting data races for OpenMP
    - Problem: May miss a race condition, our tool never reports false-negative
        - TODO: Romain, investiguer pourquoi Archer peut faire des false-negative

[12] Samuel P. Midkiff Automatic Parallelization : An Overview of Fundamental Compiler Techniques
- TODO

[13] Serial to Parallel Code Converter Tools: A Review

[14] TODO : une ref. Valgrind et opération atomique : est-ce vraiment insurmontable ?

[15] Pin3 Intel, Dynamorio (DrPin: A Dynamic binary instrumentator for mjultiple processsor architecture)

[16] Hybrid dynamic data race detection (2003)
Deux méthodes populaire:
    - lockset-based detection
    - happens before
Ils font un hybride des deux pour réduire les false-positive

[17] OMPGPT: A Generative Pre-trained Transformer Model for OpenMP
- Entrainer une IA générative GPT avec HPCorpus (un corpus de code HPC)
- OK pour du parallel for (code régulier, dense), mais quid irrégulier creux ?

[18] Cyclebite: Extracting Task Graphs From Unstructured Compute-Programs
- Source -> Markov Control Graph -> extract coarse tasks -> attribute data dependencies using epochs
- Romain: je ne comprends pas comment ils génèrent leur dépendances et leur truc epoch: est-ce qu'il considère l'ordre séquentiel = correct ?
    -> ne pas citer

[19] Efficient Detection of Determinacy Races in Cilk Programs
- {race condition, data races, ...} c Determinacy races
- Nondeterminator = cilk tool to detect determinacy races provably and accuratly
- Point commun
    - outil serial pour detécter erreur programme parallèle
        (the serial elision (or C elision) of the full Cilk program)
    - Like Tarjan’s algorithm, the SPbags algorithm uses an efficient data structure [6, Chapter 22] to manage disjoint sets of elements.
    - It uses the fact that any Cilk program can be executed on one processor in a depth-first (C-like) fashion and conforms to the semantics of the C program that results when all spawn and sync keywords are removed
    - two shadow spaces of shared memory called writer and reader.

1) Ils apportent une réponse assez générale au problème de détecter les 'determinacy races' pour des programmes Cilk restreint à 'spawn' + 'sync'
Beaucoup de points de convergences avec ce que je fais actuellement (forcer exécution séquentielle assumée correcte, algorithmie sur structure creuse, 'shadow' memory load/store, 'parallel control-flow dag'...)

2) J'ai aussi un doute si leur outil (“Nondeterminator) gère ce que j'ai présenté en E3 tout à l'heure (tâche fille accédant aux données de sa tâche mère après sa complétion - données = private / pile du thread d'exécution)
Est-ce que ce problème existe de par le modèle d'exécution de Cilk (?)

3) Un positionnement possible: on reprends les travaux “Nondeterminator" en adaptant pour des programmes OpenMP
(spawn = pragma omp task ; taskwait = sync) ; nous on ajoute "depend" dans la grammaire
L'algo interne de l'outil ("SP Bags") sort juste "determinacy race ou non" - nous on cherche aussi à fournir une suggestion pour corriger le programme
Je dois relire pour bien comprendre ce que ça fait

4) C'est prématuré de mon côté: une piste pour + d'originalité serait inclure les 'target nowait' dans la vérification
(en les compilant visant CPU, puis en virtualisant un espace mémoire disjoint, on doit pouvoir détecter des erreurs, exemple: "accès à de la mémoire (GPU) non initialisé")

5) L'outil Cilk stocke les read/write à 1 octet prêt

[20] Efficient Data Race Detection for Async-Finish Parallelism
- [19] Adapté à X10

[21] Runtime Determinacy Race Detection for OpenMP Tasks - 2018 Europar
- Cilk threads = task segments
- problèmes
    - ils confondent un peu determinacy races avec data races
    - limité à OpenM task (pas un mot sur target, parallel for...)
    - eval sur microbenchmark seulement
    - pas un mot sur 'atomic'
- bon point
    - 3.3 Happens-Before Relations Between Task Operations
        -> reprendre leur définition, et on construit un graphe où noeud = segment
        -> renommer "happens-before" en "precedes", comme Cilk le définissait
    - Algorithm. 1
        -> super, reprendre (surtout la commutativité)
    - Par rapport à Archer, ils ont le 'happens-before' sur les tâches, donc moins de faux-négatifs
- distinction
    - Instrumention LLVM compile-time
        -> exec native,
            - mais dois recompiler toute la pile soft
            - exec parallèle
                ->  ne prend pas avantage du C-Elision, donc faux-négatif possible
    -> nous, méthode robuste: PAS DE FAUX NEGATIF !! s'il y a, on detecte


[22] Parallel Data Race Detection for Task Parallel Programs with Locks
(voir relworks)

[23] Scalable and Precise Dynamic Datarace Detection for Structured Parallelism
(voir abstract)

# HISTORIQUE RAPIDE
- Cilk et Determinator (= determinacy races general, detection) (1997)
    - spawn - task
    - sync - taskwait
    position
        - depend, parallel for, ...
        - remonté d'erreur précise dans OMP

- Valgrind - How to Shadow Every Byte of Memory Used by a Program - (2007)
    - slowdown x20 (par rapport à du séquentiel)
    Position
    -> le dire

- Helgrind - https://valgrind.org/docs/manual/hg-manual.html - (2007-2009)
    - pthread <- mutex, condvar, ...
    - solution partielle, pas les infos liés aux tâches

- ThreadSanitizer – data race detection in practice (2009)
    - <=> helgrind, mais compile (= + rapide)
    - demande recompile, sinon ne voit psa tout (~x2 par rapport à //)

- Efficient Data Race Detection for Async-Finish Parallelism (X10) - (2010)
    - async / future <=> 1997

- Scalable and Precise Dynamic Datarace Detection for Structured Parallelism (2012)
    - TODO: à lire et positionner

- ARCHER: Effectively Spotting Data Races in Large OpenMP Applications - (2016) (IPDPS)
    - peut produire des faux-négatifs
    - ne voit psa tout contrairement à valgrind

- Runtime Determinacy Race Detection for OpenMP Tasks - (2018) (Europar)

- OmpSs-2 - A Toolchain to Verify the Parallelization of OmpSs-2 Applications - 2020 (= juste au niveau d'une tâche, avec suggestions de correction)


# TODO
TODO à regarder:
    - algo distribué, protocole de cohérence mémoire pour garantir ordre d'exécution
        - DSM = distributed shared memory (mémoire shared virtuel)

TODO à regarder:
    - Intersection de structure creuse
    - Arbre de recherche

TODO à regarder:
    - C-Ellision (code séquentiel = juste)
    - Cilk Memchecker (verif correctness)
    - CilkSan (determinacy race detector)
        - Efficient Detection of Determinacy Races in Cilk Programs
        - https://cilk.mit.edu/tools/

TODO: vérifier OpenMP<2013 vérification

# Series-Parallel Graph
- On n'a pas de S-P Series-Parallel Graph
- à la place on a un graph de "logical part" avec e1<e2 et e1||e2 <=> chemin ou non

# PROBLEMES ACTUELS

## LES ATOMIQUES
Comment les gérer ?

## ACCES SUR LA STACK
Exemple complètement OK : taskgrind voit 'A' et 'B' qui écrivent à la même adresse (sur la pile du thread), mais c'est pas un problème
```c
# pragma omp task
{
    # pragma omp task // A
        int x = 0;

    # pragma omp task // B
        int x = 1;

    // on s'en fou x est dépilé
}
```

Exemple complètement PAS OK : taskgrind voit 'A' et 'B' écrire à la même adresse, mais c'est un problème
```c
# pragma omp task
{
    int x;

    # pragma omp task // A
        x = 0;

    # pragma omp task // B
        x = 1;

    # pragma omp taskwait

    // 'x' vaut 0 ou 1
}
```

## LLVM specific behaviors
### Les workshare en single-threaded
ce code ne génère qu'un seul callback "work" en single threaded
```
# pragma omp for schedule(static, 1)
for (int i = 0 ; i < 4 ; ++i)
{}
```

### Pas de callback sync à la fin d'une région parallèle
```
# pragma omp parallel
{
} // pas de sync ici
```

### Pas de callback 'ompt_dispatch_section'
```
# pragma omp sections
{
    # pragma omp section
    { // pas de callback ici
    }
}
```
