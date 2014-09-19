#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern int RPN(char *str, double *pfResult);


#define RC_FAIL -1
#define RC_SUCCESS 0
#define NUL        0

#define TRUE RC_SUCCESS
#define FALSE RC_FAIL

#define MAX_LINES 4000
#define CHR0_VAL    '\0'
#define CHR0_CHR   "\0"
#define NULL_CHR   ""
#define BLNK_CHR   " "
#define EQUL_CHR   "="

typedef struct
{
    char pclLine[256];
    char pclId[256];
    char pclValue[256];
    double dfValue;
}_LINE;

typedef struct {
    _LINE rlLine[MAX_LINES];
} _FILE;

typedef struct
{
    int ilRow;
    int ilCol;
}_SPEC;

_FILE rgFile;
_SPEC rgSpec;
static int igTotalLineOfFile = 0;

static int checkInput();
static int readSpreadsheet(char *pcpFileName, _FILE *rgFile);
static void storeFile(_LINE *rpLine, char *pcpLine);
static void showFile(_FILE *rgFile, int igTotalLineOfFile);
static void buildArray(char *pcpParameter, _SPEC *rpSpec);
static void searchValue(char *pcpLine, char *pclValue);
static int GetNoOfElements(char *s, char c);
int str_fnd_fst(int c_pos, int lst_p, char *t, char *s, int *c_rck, int flg);
int str_cpy_mid ( char *s, int s_pos, int s_len, char *t, int t_pos, int t_len, char *d);
void str_trm_rgt(char *t, char *d, int meth);
int str_fnd_lst(int c_pos, int fst_p, char *t, char *s, char *d, int flg);
static void replaceValue(char *pcpLine, char *pclValue, double *dpValue);
int get_item(int nbr, char *ftx, char *buf, int b_len, char *i_sep, char *s_lim, char *e_lim);

int str_fnd_fst(int c_pos, int lst_p, char *t, char *s, int *c_rck, int flg) {
        int s_len;
        int i;

        s_len = strlen(s);

        if (lst_p < NUL) {
                lst_p = strlen(t) - 1;
        }/* end if */
        if (c_pos > lst_p){
                c_pos = lst_p + 1;
        }/* end if */

        /* search for just one character */
        if (s_len == 1) {
                while ( (c_pos <= lst_p) &&
                        ( ((flg == FALSE) && (t[c_pos] == *s)) ||
                          ((flg == TRUE)  && (t[c_pos] != *s)) ) ) {
                        c_pos++;
                }/* end while */
        }/* end if */
        else {
                /* search for a set of characters */
                while (c_pos <= lst_p) {
                        for ( i = NUL; i < s_len; i++) {
                                if ( ((flg == TRUE) && (t[c_pos] == s[i])) ||
                                     ((flg == FALSE)  && (t[c_pos] != s[i])) ) {
                                        *c_rck = i;
                                        c_pos--;
                                        /* stop while */
                                        lst_p = c_pos -1;
                                        break;
                                }/* end if */
                        }/* end for */
                        c_pos++;
                }/* end while */
        }/* end else */

        return (c_pos);
}/* END OF FUNCTION */

int GetNoOfElements(char *s, char c)
{
        int     anz;

        if (s == NULL)
                return -1;
        if (!strlen(s))
                return 0;

        anz = 0;
        while (*s)
        {
                if (*s == c)
                        anz++;
                s++;
        }

        return ++anz;
}

int str_fnd_lst(int c_pos, int fst_p, char *t, char *s, char *d, int flg) {
        int s_len;
        int i;
        s_len = strlen(s);

        if (c_pos < NUL) {
                c_pos = strlen(t);
                c_pos--;
        }/* end if */

        switch (flg) {

                /* search matching position */
                case TRUE:
                        if (s_len == 1) {
                                /* for just one character */
                                while ( (c_pos >= fst_p) &&
                                        (t[c_pos] != *s) ) {
                                        c_pos--;
                                }/* end while */
                        } /* end if */
                        else {
                                /* for a group of characters */
                                c_pos++;
                                while (c_pos >= fst_p) {
                                        c_pos--;
                                        for ( i = NUL; i < s_len; i++) {
                                                if (t[c_pos] == s[i]) {
                                                        /* stop while */
                                                        fst_p = c_pos + 1;
                                                        break;
                                                }/* end if */
                                        }/* end for */
                                }/* end while */
                        }/* end else */
                        break;

                /* search not matching position */
                case FALSE:
                        if (s_len == 1) {
                                while ( (c_pos >= fst_p) &&
                                        (t[c_pos] == *s) ) {
                                        c_pos--;
                                }/* end while */
                        }/* end if */
                        break;
                default:
                        ;
        }/* end switch */

        if ((c_pos >= NUL) && (*d != CHR0_VAL)) {
                t[c_pos] = *d;
        }/* end if */

        return (c_pos);
}/* END OF FUNCTION */

void str_trm_rgt(char *t, char *d, int meth) {
        int lst_c;
        int d_len;
        int t_len;
        int j;
        char ch[] = BLNK_CHR;

        lst_c = strlen(t);
        d_len = strlen(d);
        do {
                t_len = lst_c;
                for (j = NUL; j < d_len; j++) {
                        ch[NUL] = d[j];
                        lst_c = str_fnd_lst(-1, NUL, t, ch, NULL_CHR, FALSE);
                        lst_c++;
                        t[lst_c] = CHR0_VAL;
                }/* end for */
                if (meth == FALSE) {
                        t_len = lst_c;
                }
        } while (lst_c < t_len);
        return;
}/* END OF FUNCTION */

static int checkInput(int argc, char *argv[])
{
    char *pclFunc = "checkInput";

    if (argc < 2)
    {
        printf("%s The argc<%d> is less than 2, exit",pclFunc, argc);
        return RC_FAIL;
    }
    else
    {
        //printf("%s The argc is <%d>, filename is <%s>, exit",pclFunc, argc, argv[1]);
    }

    return RC_SUCCESS;
}

static void storeFile(_LINE *rpLine, char *pcpLine)
{
    /*char *pclFunc = "storeFile";*/

    memset(rpLine,0x00,sizeof(rpLine));
    strcpy(rpLine->pclLine,pcpLine);
}

static void showFile(_FILE *rpFile, int ipTotalLineOfRule)
{
    char *pclFunc = "showRule:";
    int ilCount = 0;

    //printf("%s There are <%d> lines, the comment are not shown:\n\n", pclFunc, ipTotalLineOfRule);
    for (ilCount = 0; ilCount < ipTotalLineOfRule; ilCount++)
    {
       // printf("%s Line[%d] <%s>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine);
    }
}

static int readSpreadsheet(char *pcpFileName, _FILE *rgFile)
{
    int ilNoLine = 0;
    char *pclFunc = "readSpreadsheet";
    char pclLine[2048];
    FILE *fp;

    if ((fp = (FILE *) fopen(pcpFileName, "r")) == (FILE *) NULL)
    {
        printf("%s Text File <%s> does not exist", pclFunc, pcpFileName);
        return RC_FAIL;
    }

    while (fgets(pclLine, 2048, fp))
    {
        /*Getting the line one by one from text file*/
        pclLine[strlen(pclLine) - 1] = '\0';
        if (pclLine[strlen(pclLine) - 1] == '\012' || pclLine[strlen(pclLine) - 1] == '\015')
        {
            pclLine[strlen(pclLine) - 1] = '\0';
        }

        /*Ignore comment*/
        if (pclLine[0] == '#')
        {
            //printf("%s Comment Line -> Ignore & Continue", pclFunc);
            continue;
        }

		if ((pclLine[0] == ' ') || (pclLine[0] == '\n') || strlen(pclLine) == 0 )
        {
            continue;
        }

        storeFile( &(rgFile->rlLine[ilNoLine++]), pclLine);
    }
    igTotalLineOfFile = ilNoLine;
    showFile(rgFile, igTotalLineOfFile);

    return RC_SUCCESS;
}


static void buildArray(char *pcpParameter, _SPEC *rpSpec)
{
    int ilRow = 0;
    int ilCol = 0;
    char *pclFunc = "buildArray";

    ilRow = pcpParameter[2] - '0';
    ilCol = pcpParameter[0] - '0';
    //printf("\n%s pcpParameter<%s> ilRow<%d> ilCol<%d>",pclFunc, pcpParameter, ilRow, ilCol);

    (*rpSpec).ilRow = ilRow;
    (*rpSpec).ilCol = ilCol;
}

static void storeID(_FILE *rpFile, _SPEC rpSpec)
{
    int ilCount = 0;

    char icLetter = 'A';
    int ilNumber = 0;

    //printf("%s There are <%d> lines, the comment are not shown:\n\n", pclFunc, igTotalLineOfFile);
    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (ilCount % (rpSpec.ilCol+1) == 0)
        {
            icLetter++;
            ilNumber = 1;
        }
        else
        {
            ilNumber++;
        }

        sprintf(rpFile->rlLine[ilCount].pclId,"%c%d",icLetter, ilNumber);
        //printf("%s Line[%d] <%s> ID <%s>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId);
    }
}

static void searchValue(char *pcpLine, char *pclValue)
{
    int ilCount = 0;

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (strcmp(rgFile.rlLine[ilCount].pclId, pcpLine)==0)
        {
            strcpy(pclValue, rgFile.rlLine[ilCount].pclLine);
        }
    }

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (strlen(rgFile.rlLine[ilCount].pclValue)==0)
        {
            strcpy(rgFile.rlLine[ilCount].pclValue, rgFile.rlLine[ilCount].pclLine);
        }
    }

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (strlen(rgFile.rlLine[ilCount].pclValue)==0)
        {
            strcpy(rgFile.rlLine[ilCount].pclValue, rgFile.rlLine[ilCount].pclLine);
        }
    }
}
static void finalReplaceValue(char *pcpLine, char *pcpValue, double *dpValue)
{
    int ilCount = 0;
    int ilCount1 = 0;
    int ilEle = 0;
    char pclEle[256]  = "\0";
    char pclTmpValue[256] = "\0";
    char pclTmp[1024] = "\0";

    ilEle = GetNoOfElements(pcpValue,' ');
    for(ilCount = 1; ilCount <= ilEle; ilCount++)
    {
        get_item(ilCount, pcpValue, pclEle, 0, " ", "\0", "\0");
        //printf("pclEle<%s>\n",pclEle);

        if(pclEle[0] >= 'A' && pclEle[0] <= 'Z')
        {
            //printf("inside\n");
            for (ilCount1 = 1; ilCount1 < igTotalLineOfFile; ilCount1++)
            {
                memset(pclTmpValue,0,sizeof(pclTmpValue));
                if(/*strcmp(rgFile.rlLine[ilCount1].pclLine, pclEle) == 0 ||*/ strcmp(rgFile.rlLine[ilCount1].pclId, pclEle) == 0)
                {
                    sprintf(pclTmpValue,"%.5f",rgFile.rlLine[ilCount1].dfValue);
                    //printf("pclTmpValue<%s>\n",pclTmpValue);
                    if(strlen(pclTmp) == 0)
                    {
                        strcat(pclTmp,pclTmpValue);
                    }
                    else
                    {
                        strcat(pclTmp," ");
                        strcat(pclTmp,pclTmpValue);
                    }
                }
            }
        }
        else
        {
          if(strlen(pclTmp) == 0)
            {
                strcat(pclTmp,pclEle);
            }
            else
            {
                strcat(pclTmp," ");
                strcat(pclTmp,pclEle);
            }
        }
    }

    strcpy(pcpValue,pclTmp);

    RPN(pcpValue, dpValue);
}

static void replaceValue(char *pcpLine, char *pclValue, double *dpValue)
{
    int ilCount = 0;

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if(strcmp(rgFile.rlLine[ilCount].pclLine, pclValue) == 0)
        {
            *dpValue = rgFile.rlLine[ilCount].dfValue;
            //sprintf(pclValue,"%.5f",*dpValue);
            //printf("Line<%s> pclValue <%s> value<%.5f>\n",rgFile.rlLine[ilCount].pclLine, pclValue, *dpValue);
        }
    }
}

void calculateValue(_FILE *rpFile, _SPEC rpSpec)
{
    char *pclFunc = "calculateValue";
    int ilCount = 0;
    int ilEle = 0;
    int ilCount1 = 0;
    char pclEle[256] = "\0";
    printf("\n");

    int iliFlag = 0;

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        searchValue(rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclValue);

        //printf("%s Line[%d] <%s> ID <%s> Value<%s> value<%.5f>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId, rpFile->rlLine[ilCount].pclValue, rpFile->rlLine[ilCount].dfValue);

    }

    printf("\n");
    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (strstr(rpFile->rlLine[ilCount].pclValue,"A") ==0 && strstr(rpFile->rlLine[ilCount].pclValue,"B") ==0)
        {
            RPN(rpFile->rlLine[ilCount].pclValue, &(rpFile->rlLine[ilCount].dfValue));
        }

        //printf("%s Line[%d] <%s> ID <%s> Value<%s> value<%.5f>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId, rpFile->rlLine[ilCount].pclValue, rpFile->rlLine[ilCount].dfValue);
    }

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        replaceValue(rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclValue, &(rpFile->rlLine[ilCount].dfValue));

         //printf("%s Line[%d] <%s> ID <%s> Value<%s> value<%.5f>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId, rpFile->rlLine[ilCount].pclValue, rpFile->rlLine[ilCount].dfValue);
    }

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        if (rpFile->rlLine[ilCount].dfValue == (double)0)
        {
            finalReplaceValue(rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclValue, &(rpFile->rlLine[ilCount].dfValue));
        }

        //printf("%s Line[%d] <%s> ID <%s> Value<%s> value<%.5f>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId, rpFile->rlLine[ilCount].pclValue, rpFile->rlLine[ilCount].dfValue);

    }

    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        sprintf(rpFile->rlLine[ilCount].pclValue, "%.5f",rpFile->rlLine[ilCount].dfValue);

        //printf("%s Line[%d] <%s> ID <%s> Value<%s> value<%.5f>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine, rpFile->rlLine[ilCount].pclId, rpFile->rlLine[ilCount].pclValue, rpFile->rlLine[ilCount].dfValue);
    }


    for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
    {
        ilEle = GetNoOfElements(rpFile->rlLine[ilCount].pclValue,' ');
        for(ilCount1 = 1; ilCount1 <= ilEle; ilCount1++)
        {
            memset(pclEle,0,sizeof(pclEle));
            get_item(ilCount1, rpFile->rlLine[ilCount].pclValue, pclEle, 0, " ", "\0", "\0");
            //printf("%s pclEle<%s>\n",pclFunc, pclEle);

            if((pclEle[0] <= 'Z' && pclEle[0] >='A'))
            {
                printf("cyclic dependencies\n");
                iliFlag = 1;
                break;
            }
        }
    }

    if(iliFlag == 0)
    {
        for (ilCount = 1; ilCount < igTotalLineOfFile; ilCount++)
        {
            printf("%.5f\n", rpFile->rlLine[ilCount].dfValue);
        }
    }
}

int get_item(int nbr, char *ftx, char *buf, int b_len, char *i_sep, char *s_lim, char *e_lim) {

        int  rc = TRUE;

        int  gt_rc;
        int i;

        int  f_pos = NUL;
        int  fst_i = NUL;

        int  nxt_i;
        int  nxt_k;
        int  fnd_k = NUL;

        int  lst_i;

        int  t_len;
        int  d_len;
        int  dmmy = NUL;

        d_len = strlen(s_lim);

        lst_i = strlen(ftx) - 1;
        nxt_i = lst_i + 1;

        /* get begin of next item-region */
        /* (fnd_k gets the index of found char in s_lim) */
        nxt_k = str_fnd_fst(fst_i, lst_i, ftx, s_lim, &fnd_k, TRUE);

        for (i=NUL; i<nbr; i++) {

                /* store first position */
                /* of current item      */
                f_pos = fst_i;

                /* position of next separator */
                nxt_i = str_fnd_fst(fst_i, lst_i, ftx, &i_sep[NUL], &dmmy, TRUE);

                while (nxt_i > nxt_k) {
                        /* position in an item-region ?   */
                        /* skip to the end of that region */
                        /* (fnd_k is the char-index of the region) */
                        fst_i = nxt_k + 1;
                        nxt_k = str_fnd_fst(fst_i, lst_i, ftx, &e_lim[fnd_k], &dmmy, TRUE);

                        /* get next separator position */
                        fst_i = nxt_k + 1;
                        nxt_i = str_fnd_fst(fst_i, lst_i, ftx, &i_sep[NUL], &dmmy, TRUE);

                        /* get begin of next item-region */
                        nxt_k = str_fnd_fst(fst_i, lst_i, ftx, s_lim, &fnd_k, TRUE);
                }/* end while */


                fst_i = nxt_i + 1;

        }/* end for */

        t_len = nxt_i - f_pos;
        if (b_len == 0){
                b_len = t_len + 1;
        }/* end if */
        if (t_len > 0){
                gt_rc = str_cpy_mid(buf, NUL, b_len, ftx, f_pos, t_len, CHR0_CHR);
        }/* end if */
        else {
                buf[0] = 0x00;
        }/* end else */

        if ((f_pos > lst_i) || (nbr < 1)) {
                rc = FALSE;
        }/* end if */

        return (rc);

}/* END OF FUNCTION */

int str_cpy_mid ( char *s, int s_pos, int s_len, char *t, int t_pos, int t_len, char *d) {
        int rc = -10;

        if (s_len < 1) {
                s_len = strlen(s);
        }/* end if */

        if ((s_pos >= NUL) && (s_pos < s_len)) {

                if (t_len < 1){
                        t_len = strlen(t) - t_pos;
                }/* end if */

                if ( (s_pos + t_len) > s_len ) {
                        t_len = s_len - s_pos;
                }/* end if */

                while (t_len > NUL) {
                        s[s_pos] = t[t_pos];
                        t_pos++;
                        s_pos++;
                        t_len--;
                }/* end while */

                if (s_pos < s_len) {
                        s[s_pos] = d[NUL];
                }/* end if */

                rc = s_pos;

        }/* end if */

        return (rc);
}/* END OF FUNCTION */
#endif
