#include "LKH.h"

/*
 * After the candidate set has been created the FindTour function is called
 * a predetermined number of times (Runs).
 *
 * FindTour performs a number of trials, where in each trial it attempts
 * to improve a chosen initial tour using the modified Lin-Kernighan edge
 * exchange heuristics.
 *
 * Each time a better tour is found, the tour is recorded, and the candidates
 * are reorderded by the AdjustCandidateSet function. Precedence is given to
 * edges that are common to two currently best tours. The candidate set is
 * extended with those tour edges that are not present in the current set.
 * The original candidate set is re-established at exit from FindTour.
 */

/*
 在确定候选边集合以后，FindTour()函数会被调用Runs次(FindTour()函数在这样一个for循环里面:for (Run = 1; Run <= Runs; Run++) )。
 FindTour()函数每次执行的时候，会使用LKH算法修正可行解
 每当找到一个更好的可行解时，就会记录下来，然后使用AdjustCandidateSet()函数重新排序候选边集合
 如果一条边被两条已存在的最优路径包含，那么它的优先级就越高。
 候选边集合在扩展的时候，会从不在当前tour中的边中选择。
 在FindTour()函数退出之前，原始的候选边集合都会被重新建立。
 这个函数会返回最优解的权重
 */

static void SwapCandidateSets();
static GainType OrdinalTourCost;

GainType FindTour()
{
    GainType Cost;
    Node *t;
    int i;
    double EntryTime = GetTime();

    t = FirstNode;
    //初始化所有节点的OldPred，OldSuc，NextBestSuc和BestSuc 为 0;
    do
        t->OldPred = t->OldSuc = t->NextBestSuc = t->BestSuc = 0;
    while ((t = t->Suc) != FirstNode);
    // 这个if语句只会在FindTour()函数第一次运行时进入
    if (Run == 1 && Dimension == DimensionSaved) {
        // 初始化OrdinalTourCost
        OrdinalTourCost = 0;
        for (i = 1; i < Dimension; i++)
            OrdinalTourCost += C(&NodeSet[i], &NodeSet[i + 1])
                               - NodeSet[i].Pi - NodeSet[i + 1].Pi;
        OrdinalTourCost += C(&NodeSet[Dimension], &NodeSet[1])
                           - NodeSet[Dimension].Pi - NodeSet[1].Pi;
        OrdinalTourCost /= Precision;
    }
    BetterCost = PLUS_INFINITY;
    if (MaxTrials > 0)
        // HashInitialize(HTable)会把传入的HTable清空
        HashInitialize(HTable);
    // 这个else语句永远不会进入
    else {
        Trial = 1;
        ChooseInitialTour();
    }
    //运行MaxTrials(节点个数)次LKH算法
    for (Trial = 1; Trial <= MaxTrials; Trial++) {
        // 当程序运行时间即将超过规定时间时，终止for循环
        if (GetTime() - EntryTime >= TimeLimit) {
            if (TraceLevel >= 1)
                printff("*** Time limit exceeded ***\n");
            break;
        }
        // 任意选择一个初始点
        if (Dimension == DimensionSaved)
            FirstNode = &NodeSet[1 + Random() % Dimension];
        // 这个else语句永远不会运行
        else
            for (i = Random() % Dimension; i > 0; i--)
                FirstNode = FirstNode->Suc;
        // ChooseInitialTour()函数会按照顺序生成一个伪随机的初始路径
        ChooseInitialTour();
        // LinKernighan()函数通过opt交换来修正可行解。这个函数会返回修正解权重        
        Cost = LinKernighan();
        if (FirstNode->BestSuc) {
            //将当前最优路径合并
            t = FirstNode;
            while ((t = t->Next = t->BestSuc) != FirstNode);
             // MergeWithTour()函数将给定的路径合并来得到更短的tour
            Cost = MergeWithTour();
        }
        if (Dimension == DimensionSaved && Cost >= OrdinalTourCost &&
                BetterCost > OrdinalTourCost) {
            //按照顺序依次合并所有路径
            for (i = 1; i < Dimension; i++)
                NodeSet[i].Next = &NodeSet[i + 1];
            NodeSet[Dimension].Next = &NodeSet[1];
            Cost = MergeWithTour();
        }
        if (Cost < BetterCost) {
            //打印
            if (TraceLevel >= 1) {
                printff("* %d: Cost = " GainFormat, Trial, Cost);
                if (Optimum != MINUS_INFINITY && Optimum != 0)
                    printff(", Gap = %0.4f%%",
                            100.0 * (Cost - Optimum) / Optimum);
                printff(", Time = %0.2f sec. %s\n",
                        fabs(GetTime() - EntryTime),
                        Cost < Optimum ? "<" : Cost == Optimum ? "=" : "");
            }
            BetterCost = Cost;
            // 这个函数会把这个更好的解记录在BetterTour[]数组中。如果这个数组已经有值，就把原来的
            // 值存在NextBestSuc[]数组中，然后才更新。
            RecordBetterTour();
            if (Dimension == DimensionSaved && BetterCost < BestCost)
                WriteTour(OutputTourFileName, BetterTour, BetterCost);
            if (StopAtOptimum && BetterCost == Optimum)
                break;
            AdjustCandidateSet();
            HashInitialize(HTable);
            HashInsert(HTable, Hash, Cost);
        } else if (TraceLevel >= 2)
            printff("  %d: Cost = " GainFormat ", Time = %0.2f sec.\n",
                    Trial, Cost, fabs(GetTime() - EntryTime));
        /* Record backbones if wanted */
        //不进入
        if (Trial <= BackboneTrials && BackboneTrials < MaxTrials) {
            SwapCandidateSets();
            AdjustCandidateSet();
            if (Trial == BackboneTrials) {
                if (TraceLevel >= 1) {
                    printff("# %d: Backbone candidates ->\n", Trial);
                    CandidateReport();
                }
            } else
                SwapCandidateSets();
        }
    }
    //不会进入
    if (BackboneTrials > 0 && BackboneTrials < MaxTrials) {
        if (Trial > BackboneTrials ||
                (Trial == BackboneTrials &&
                 (!StopAtOptimum || BetterCost != Optimum)))
            SwapCandidateSets();
        t = FirstNode;
        do {
            free(t->BackboneCandidateSet);
            t->BackboneCandidateSet = 0;
        } while ((t = t->Suc) != FirstNode);
    }
    t = FirstNode;
    //这个if无意义
    if (Norm == 0) {
        do
            t = t->BestSuc = t->Suc;
        while (t != FirstNode);
    }
    do
        (t->Suc = t->BestSuc)->Pred = t;
    while ((t = t->BestSuc) != FirstNode);
    //不会进入
    if (Trial > MaxTrials)
        Trial = MaxTrials;
    // ResetCandidates()函数会移除候选边集合中正在使用的边和Alpha等于正无穷的边，然后将集合中的边重新排序
    ResetCandidateSet();
    return BetterCost;
}

/*
 * The SwapCandidateSets function swaps the normal and backbone candidate sets.
 */
// 不会运行
static void SwapCandidateSets()
{
    printf("我被运行\n");
    Node *t = FirstNode;
    do {
        Candidate *Temp = t->CandidateSet;
        t->CandidateSet = t->BackboneCandidateSet;
        t->BackboneCandidateSet = Temp;
    } while ((t = t->Suc) != FirstNode);
}
