#include <stdio.h>
#include <stdlib.h>
#define N 8
int dRow[8]={-1,-1,-1,0,0,1,1,1};
int dCol[8]={-1,0,1,-1,1,-1,0,1};
void readBoard(char board[N][N+1]){
    for(int i=0;i<N;i++){
        if(scanf("%8s",board[i])!=1){
            fprintf(stderr,"Input error\n");
            exit(1);
        }
    }
}
int absVal(int x){
    if(x<0) return -x;
    return x;
}
int hasValidMove(char board[N][N+1],char player){
    for(int rr=0;rr<N;rr++){
        for(int cc=0;cc<N;cc++){
            if(board[rr][cc]!=player) continue;
            for(int d=0;d<8;d++){
                for(int s=1;s<=2;s++){
                    int nr=rr+dRow[d]*s;
                    int nc=cc+dCol[d]*s;
                    if(nr<0){ continue; }
                    if(nr>=N){ continue; }
                    if(nc<0){ continue; }
                    if(nc>=N){ continue; }
                    if(s==2){
                        int mr=rr+dRow[d];
                        int mc=cc+dCol[d];
                        if(board[mr][mc]=='R') continue;
                        if(board[mr][mc]=='B') continue;
                    }
                    if(board[nr][nc]=='.'){
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
int isValidMove(char board[N][N+1],int r1,int c1,int r2,int c2,char current){
    char opponent;
    if(current=='R') opponent='B'; else opponent='R';
    if(r1<0) return 0;
    if(r1>=N) return 0;
    if(c1<0) return 0;
    if(c1>=N) return 0;
    if(r2<0) return 0;
    if(r2>=N) return 0;
    if(c2<0) return 0;
    if(c2>=N) return 0;
    if(board[r1][c1]!=current) return 0;
    if(board[r2][c2]!='.') return 0;
    int dr=r2-r1;
    int dc=c2-c1;
    int absDr=absVal(dr);
    int absDc=absVal(dc);
    int maxD;
    if(absDr>absDc) maxD=absDr; else maxD=absDc;
    if(maxD!=1 && maxD!=2) return 0;
    if(absDr!=0 && absDc!=0 && absDr!=absDc) return 0;
    if(maxD==2){
        int mr=r1+dr/2;
        int mc=c1+dc/2;
        if(board[mr][mc]=='R') return 0;
        if(board[mr][mc]=='B') return 0;
    }
    return 1;
}
void applyMove(char board[N][N+1],int r1,int c1,int r2,int c2,char current){
    char opponent;
    if(current=='R') opponent='B'; else opponent='R';
    int dr=r2-r1;
    int dc=c2-c1;
    int absDr=absVal(dr);
    int absDc=absVal(dc);
    int maxD;
    if(absDr>absDc) maxD=absDr; else maxD=absDc;
    if(maxD==2){
        board[r1][c1]='.';
    }
    board[r2][c2]=current;
    for(int d=0;d<8;d++){
        int nr=r2+dRow[d];
        int nc=c2+dCol[d];
        if(nr<0) continue;
        if(nr>=N) continue;
        if(nc<0) continue;
        if(nc>=N) continue;
        if(board[nr][nc]==opponent){
            board[nr][nc]=current;
        }
    }
}
void countPieces(char board[N][N+1],int*cntR,int*cntB){
    *cntR=0;
    *cntB=0;
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            if(board[i][j]=='R'){
                (*cntR)++;
            }
            if(board[i][j]=='B'){
                (*cntB)++;
            }
        }
    }
}
int countEmpty(char board[N][N+1]){
    int cnt=0;
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            if(board[i][j]=='.'){
                cnt++;
            }
        }
    }
    return cnt;
}
void printBoard(char board[N][N+1]){
    for(int i=0;i<N;i++){
        printf("%.*s\n",N,board[i]);
    }
}
void printResult(int cntR,int cntB){
    if(cntR>cntB){
        printf("Red\n");
    } else {
        if(cntB>cntR){
            printf("Blue\n");
        } else {
            printf("Draw\n");
        }
    }
}
int main(){
    char board[N][N+1];
    readBoard(board);
    int moveCount;
    if(scanf("%d",&moveCount)!=1) {
        fprintf(stderr,"Invalid input at turn 0\n");
        return 1;
    }
    if(moveCount<0){
        fprintf(stderr,"Invalid input at turn 0\n");
        return 1;
    }
    char current='R';
    int turn=1;
    int consecutivePasses=0;
    while(turn<=moveCount){
        int r1,c1,r2,c2;
        if(scanf("%d %d %d %d",&r1,&c1,&r2,&c2)!=4){
            printf("Invalid move at turn %d\n",turn);
            return 0;
        }
        if(r1==0){
            if(c1==0){
                if(r2==0){
                    if(c2==0){
                        if(hasValidMove(board,current)==1){
                            printf("Invalid move at turn %d\n",turn);
                            return 0;
                        }
                        consecutivePasses++;
                        if(consecutivePasses==2){
                            break;
                        }
                        if(current=='R'){
                            current='B';
                        } else {
                            current='R';
                        }
                        turn++;
                        continue;
                    }
                }
            }
        }
        // 0-based 변환
        r1--;
        c1--;
        r2--;
        c2--;
        consecutivePasses=0;
        if(isValidMove(board,r1,c1,r2,c2,current)==0){
            printf("Invalid move at turn %d\n",turn);
            return 0;
        }
        applyMove(board,r1,c1,r2,c2,current);
        if(current=='R'){
            current='B';
        } else {
            current='R';
        }
        turn++;
        int cntR,cntB;
        countPieces(board,&cntR,&cntB);
        int emptyCnt=countEmpty(board);
        if(cntR==0){
            break;
        }
        if(cntB==0){
            break;
        }
        if(emptyCnt==0){
            break;
        }
    }
    printBoard(board);
    int cntR,cntB;
    countPieces(board,&cntR,&cntB);
    printResult(cntR,cntB);
    return 0;
}
