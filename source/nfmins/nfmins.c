/* ----------------------------- MNI Header -----------------------------------
@NAME       : nfmins
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: A CMEX replacement for the MATLAB fmins function.  We are
              striving for speed here.        
@METHOD     : A CMEX program
@GLOBALS    : 
@CALLS      : 
@CREATED    : October 29, 1993 by Mark Wolforth
@MODIFIED   : 
---------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "mex.h"

#define TRUE 1
#define FALSE 0

#define min(A, B) ((A) < (B) ? (A) : (B))
#define max(A, B) ((A) > (B) ? (A) : (B))
#define abs(A)    ((A) < 0 ? (A*(-1)) : (A))

#define PROGNAME "nfmins"

/*
 * Constants to check for argument number and position
 */


#define MIN_IN_ARGS        2
#define MAX_IN_ARGS        14

#define FUNFCN             prhs[0]
#define START              prhs[1]
#define OPTIONS            prhs[2]
#define GRAD               prhs[3]    /* Ignored */

#define ALPHA              1
#define BETA               0.5
#define GAMMA              2

#define FUNCVAL            (numvars)

typedef int Boolean;

Boolean progress;
char    *ErrMsg ;         /*
                           * set as close to the occurence of the
                           * error as possible
                           */


/* ----------------------------- MNI Header -----------------------------------
@NAME       : ErrAbort
@INPUT      : msg - character to string to print just before aborting
              PrintUsage - whether or not to print a usage summary before
                aborting
              ExitCode - one of the standard codes from mierrors.h -- NOTE!  
                this parameter is NOT currently used, but I've included it for
                consistency with other functions named ErrAbort in other
                programs
@OUTPUT     : none - function does not return!!!
@RETURNS    : 
@DESCRIPTION: Optionally prints a usage summary, and calls mexErrMsgTxt with
              the supplied msg, which ABORTS the mex-file!!!
@METHOD     : 
@GLOBALS    : requires PROGNAME macro
@CALLS      : standard mex functions
@CREATED    : 93-6-6, Greg Ward
@MODIFIED   : 
---------------------------------------------------------------------------- */
void ErrAbort (char msg[], Boolean PrintUsage, int ExitCode)
{
   if (PrintUsage)
   {
      (void) mexPrintf ("Usage: %s ('F', X0)\n", PROGNAME);
      (void) mexPrintf ("Compatible with MATLAB's fmins function.\n");
   }
   (void) mexErrMsgTxt (msg);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : CreateLocalMatrix
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
double **CreateLocalMatrix(int rows, int cols)
{
    int i;
    double **matrix;

    matrix = (double **) mxCalloc (rows, sizeof(double *));
    if (matrix == NULL)
    {
        ErrAbort("Out of memory!", FALSE, -1);
    }
    
    for (i=0; i<rows; i++)
    {
        matrix[i] = (double *) mxCalloc (cols, sizeof(double));
        if (matrix[i] == NULL)
        {
            ErrAbort("Out of memory!", FALSE, -1);
        }
    }
    
    return (matrix);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : GetArguments
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void GetArguments(Matrix *Start, Matrix *Options, int nrhs, int *numvars,
                  Boolean *progress, int *maxiter, double *tol,
                  double *tol2)
{
    double *options;
    int numoptions;

    *numvars = max(mxGetM(Start),mxGetN(Start));

    printf ("Number of variables: %d\n", *numvars);


    if (nrhs > 2)
    {
	if (min(mxGetM(Options),mxGetN(Options)) != 1)
	{
	    strcpy (ErrMsg, "Argument 3 must be a vector.");
	    ErrAbort (ErrMsg, TRUE, -1);
	}
    
	numoptions = max(mxGetM(Options),mxGetN(Options));
	options = mxGetPr(Options);
    
	*progress = FALSE;
	*maxiter = 200 * (*numvars);
	*tol = 0.0001;
	*tol2 = 0.0001;
	
	if (numoptions > 0)
	{
	    if (options[0] != 0)
	    {
		*progress = TRUE;
	    }
	    if (numoptions > 1)
	    {
		*tol = options[1];
	    }
	    if (numoptions > 2)
	    {
		*tol2 = options[2];
	    }
	    if (numoptions > 14)
	    {
		*maxiter = (int)options[13];
	    }
	}
    }
    else 
    {
	*progress = FALSE;
	*maxiter = 200 * (*numvars);
	*tol = 0.0001;
	*tol2 = 0.0001;
    }
}    


/* ----------------------------- MNI Header -----------------------------------
@NAME       : CopyVector
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void CopyVector(double vec1[], const double vec2[], int size)
{
    int i;
    
    for (i=0; i<size; i++)
    {
        vec1[i] = vec2[i];
    }
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : SortSimplex
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void SortSimplex (double **simplex, int numvars)
{
    int i;
    Boolean bubbled;
    double *pointer;
    
    bubbled = TRUE;

    while (bubbled)
    {
        bubbled = FALSE;
        for (i=0; i<(numvars-1); i++)
        {
            if (simplex[i][FUNCVAL] > simplex[i+1][FUNCVAL])
            {
                pointer = simplex[i];
                simplex[i] = simplex[i+1];
                simplex[i+1] = pointer;
                bubbled = TRUE;
            }
        }
    }    
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : GetStartingSimplex
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void GetStartingSimplex(double start[], int numvars,
                        char *funfcn, Matrix *arguments[], int numargs,
                        double **simplex)
{
    Matrix *answer[1];
    int i,j;

    printf ("Creating a matrix for the function return.\n");
    
    answer[0] = mxCreateFull(1,1,REAL);

    printf ("Filling the first vertex.\n");

    for (i=0; i<numvars; i++)
    {
        simplex[0][i] = 0.9 * start[i];
    }

    printf ("Filling the first function value.\n");

    mxSetPr(arguments[0], simplex[0]);
    mexCallMATLAB(1, answer, numargs, arguments, funfcn);
    simplex[0][FUNCVAL] = mxGetScalar(answer[0]);

    printf ("First vertex is:\n");
    for (i=0; i<numvars; i++)
    {
	printf ("   %lf\n", simplex[0][i]);
    }
    printf ("First function value is:\n");
    printf ("    %lf\n", simplex[0][FUNCVAL]);
    printf ("Filling the remaining vertices.\n");
    
    for (i=1; i<(numvars+1); i++)
    {

	printf ("Copying a starting vector.\n");

        CopyVector(simplex[i], start, numvars);

	printf ("Comparing element (%d,%d) to zero.\n", i, (i-1));

        if (simplex[i][i-1] != 0)
        {
	    
	    printf ("Okay, set it equal to 1.1 times itself.\n");

            simplex[i][i-1] *= 1.1;
        }
        else
        {

	    printf ("Set it equal to 0.1.\n");

            simplex[i][i-1] = 0.1;
        }

	printf ("Get the function value for this vertex.\n");

        mxSetPr(arguments[0], simplex[i]);
        mexCallMATLAB(1, answer, numargs, arguments, funfcn);
        simplex[i][FUNCVAL] = mxGetScalar(answer[0]);

	printf ("Vertex is:\n");
	for (j=0; j<numvars; j++)
	{
	    printf ("   %lf\n", simplex[i][j]);
	}
	printf ("Function value is:\n");
	printf ("    %lf\n", simplex[i][FUNCVAL]);

    }

    SortSimplex(simplex, numvars);

}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : Terminated
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
Boolean Terminated (double **simplex, int numvars, double tol, double tol2)
{
    int i,j;
    
    for (i=1; i<(numvars+1); i++)
    {
        for (j=0; j<numvars; j++)
        {
            if (abs(simplex[i][j] - simplex[0][j]) > tol)
            {
                return (FALSE);
            }
        }
        if (abs(simplex[i][FUNCVAL] - simplex[0][FUNCVAL]) > tol2)
        {
            return (FALSE);
        }
    }
    return (TRUE);
}


/* ----------------------------- MNI Header -----------------------------------
@NAME       : MinimizeSimplex
@INPUT      : 
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION:               
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void MinimizeSimplex (double **simplex, char *funfcn, Matrix *arguments[],
                      int numargs, int numvars, int maxiter, double tol,
		      double tol2, double minimum[], double *finalvalue)
{
    Matrix *answer[1];
    int i,j;
    int count;
    double temp_double;
    double *temp_vector;
    double *vbar;
    double *vr;
    double fr;
    double *vk;
    double fk;
    double *ve;
    double fe;
    double *vt;
    double ft;
    double *vc;
    double fc;
    

    answer[0] = mxCreateFull(1,1,REAL);
    temp_vector = (double *) mxCalloc (numvars, sizeof (double));
    vbar = (double *) mxCalloc (numvars, sizeof (double));
    vr = (double *) mxCalloc (numvars, sizeof (double));
    vk = (double *) mxCalloc (numvars, sizeof (double));
    ve = (double *) mxCalloc (numvars, sizeof (double));
    vt = (double *) mxCalloc (numvars, sizeof (double));
    vc = (double *) mxCalloc (numvars, sizeof (double));

    count = numvars+1;
    
    while (count < maxiter)
    {
	
        if (Terminated(simplex, numvars, tol, tol2))
        {
	    printf ("Terminated after %d iterations.\n", count);
            break;
        }

        for (i=0; i<numvars; i++)
        {
            temp_double = 0;
            for (j=0; j<numvars; j++)
            {
                temp_double += simplex[j][i];
            }
            vbar[i] = temp_double / numvars;
            vr[i] = ((1+ALPHA)*vbar[i]) - (ALPHA*simplex[numvars][i]);
        }           
        
        mxSetPr(arguments[0], vr);
        mexCallMATLAB(1, answer, numargs, arguments, funfcn); 
        fr = mxGetScalar(answer[0]);

        count++;

        CopyVector (vk, vr, numvars);
        fk = fr;
        
        if (fr < simplex[numvars-1][FUNCVAL])
        {
            if (fr < simplex[0][FUNCVAL])
            {
                for (i=0; i<numvars; i++)
                {
                    ve[i] = GAMMA*vr[i] + (1-GAMMA)*vbar[i];
                }
                
                mxSetPr(arguments[0], ve);
                mexCallMATLAB(1, answer, numargs, arguments, funfcn); 
                fe = mxGetScalar(answer[0]);

                count++;
                
                if (fe < simplex[0][FUNCVAL])
                {
                    CopyVector(vk,ve,numvars);
                    fk = fe;
                }
            }
        }
        else 
        {
            CopyVector(vt,simplex[numvars], numvars);
            ft = simplex[numvars][FUNCVAL];
            if (fr < ft)
            {
                CopyVector(vt,vr,numvars);
                ft = fr;
            }
            
            for (i=0; i<numvars; i++)
            {
                vc[i] = BETA*vt[i] + (1-BETA)*vbar[i];
            }
                
            mxSetPr(arguments[0], vc);
            mexCallMATLAB(1, answer, numargs, arguments, funfcn);     
            fc = mxGetScalar(answer[0]);
            
            count++;
            
            if (fc < simplex[numvars-1][FUNCVAL])
            {
                CopyVector(vk,vc,numvars);
                fk = fc;
            }
            else 
            {
                for (i=1; i<numvars; i++)
                {
                    for (j=0; j<numvars; j++)
                    {
                        temp_vector[j] = (simplex[0][j] + simplex[i][j])/2;
                    }
                    CopyVector(simplex[i], temp_vector, numvars);
                                
                    mxSetPr(arguments[0], simplex[i]);
                    mexCallMATLAB(1, answer, numargs, arguments, funfcn);     
                    simplex[i][FUNCVAL] = mxGetScalar(answer[0]);
                    
                }
                
                count += numvars - 1;

                for (i=0; i<numvars; i++)
                {
                    vk[i] = (simplex[0][i] + simplex[numvars+1][i])/2;
                }
                
                mxSetPr(arguments[0], vk);
                mexCallMATLAB(1, answer, numargs, arguments, funfcn); 
                fk = mxGetScalar(answer[0]);
                
                count++;
                
            }
        }
        
        for (i=0; i<numvars; i++)
        {
            simplex[numvars][i] = vk[i];
        }
        simplex[numvars][FUNCVAL] = fk;
        
        SortSimplex (simplex, numvars);
    }
    if (count >= maxiter)
    {
	printf ("Maximum number of iterations exceeded!\n");
    }	

    CopyVector (minimum, simplex[0], numvars);
    *finalvalue = simplex[0][FUNCVAL];

}

    

/* ----------------------------- MNI Header -----------------------------------
@NAME       : mexFunction
@INPUT      : nlhs, nrhs - number of output/input arguments (from MATLAB)
              prhs - actual input arguments 
@OUTPUT     : plhs - actual output arguments
@RETURNS    : (void)
@DESCRIPTION: 
@METHOD     : 
@GLOBALS    : ErrMsg
@CALLS      : 
@CREATED    : 
@MODIFIED   : 
---------------------------------------------------------------------------- */
void mexFunction(int    nlhs,
                 Matrix *plhs[],
                 int    nrhs,
                 Matrix *prhs[])
{
    int numvars;
    int numargs;
    int maxiter;
    int i;
    char *funfcn;
    Matrix *arguments[20];
    double tol;
    double tol2;
    double *start;
    double **simplex;
    double *minimum;
    double finalvalue;
    double *return_argument;
    

    ErrMsg = (char *) mxCalloc (256, sizeof (char));
    
    /* First make sure a valid number of arguments was given. */


    printf ("Checking input arguments.\n");
    
    if ((nrhs < MIN_IN_ARGS) || (nrhs > MAX_IN_ARGS))
    {
        strcpy (ErrMsg, "Incorrect number of arguments.");
        ErrAbort (ErrMsg, TRUE, -1);
    }

    GetArguments(START, OPTIONS, nrhs, &numvars, &progress, &maxiter,
                 &tol, &tol2);

    printf ("Got the input arguments:\n");
    printf ("   Number of vars: %d\n", numvars);
    printf ("   Max. Iteration: %d\n", maxiter);
    printf ("   Tolerance 1   : %lf\n", tol);
    printf ("   Tolerance 2   : %lf\n", tol2);

    /*
     * Get the function name
     */

    funfcn = (char *) mxCalloc (max(mxGetM(FUNFCN),mxGetN(FUNFCN))+2,
                                sizeof(char));
    mxGetString(FUNFCN, funfcn, max(mxGetM(FUNFCN),mxGetN(FUNFCN))+1);

    printf ("   Function name : %s\n", funfcn);	

    /*
     * Get any additional function arguments
     */


    if (nrhs > 4)
    {
	numargs = (nrhs - 4) + 1;
    }
    else 
    {
	numargs = 1;
    }

    printf ("Number of function arguments: %d\n", numargs);

    if (numargs > 1)
    {
        for (i=1; i<(nrhs-4+1); i++)
        {
            arguments[i] = prhs[i+3];
        }
    }
    arguments[0] = mxCreateFull (1,1,REAL);
    mxSetM (arguments[0], 1);
    mxSetN (arguments[0], numvars);


    /*
     * Now we need a starting simplex close to the initial
     * point given by the user.
     */

    start = mxGetPr (START);
    simplex = CreateLocalMatrix (numvars+1, numvars+1);

    printf ("Getting the starting simplex.\n");

    GetStartingSimplex (start, numvars, funfcn, arguments, numargs, simplex);

    printf ("Minimizing the simplex.\n");

    minimum = (double *) mxCalloc (numvars, sizeof (double));
    MinimizeSimplex (simplex, funfcn, arguments, numargs, numvars, maxiter,
                     tol, tol2, minimum, &finalvalue);

    plhs[0] = mxCreateFull(1, numvars, REAL);
    return_argument = mxGetPr (plhs[0]);

    CopyVector (return_argument, minimum, numvars);

}     /* mexFunction */