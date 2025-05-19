#include <stdio.h>
#include <assert.h>

#include "sse.h"
#include "neuralnet.h"

#if USE_SSE_VECTORIZE

#if defined(DISABLE_SSE_TEST)

int SSE_Supported(void)
{
  return 1;
}

#else

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

static int CheckSSE(void)
{
  int result;
#if defined(__APPLE__) || defined(__FreeBSD__)
    size_t length = sizeof( result );
#if defined(__APPLE__)
    int error = sysctlbyname("hw.optional.sse", &result, &length, NULL, 0);
#endif
#if defined(__FreeBSD__)
    int error = sysctlbyname("hw.instruction_sse", &result, &length, NULL, 0);
#endif
    if ( 0 != error ) result = 0;
    return result;

#else

	asm (
		/* Check if cpuid is supported (can bit 21 of flags be changed) */
		"mov $1, %%eax\n\t"
		"shl $21, %%eax\n\t"
		"mov %%eax, %%edx\n\t"

		"pushfl\n\t"
		"popl %%eax\n\t"
		"mov %%eax, %%ecx\n\t"
		"xor %%edx, %%eax\n\t"
		"pushl %%eax\n\t"
		"popfl\n\t"

		"pushfl\n\t"
		"popl %%eax\n\t"
		"xor %%ecx, %%eax\n\t"
		"test %%edx, %%eax\n\t"
		"jnz 1f\n\t"
		/* Failed (non-pentium compatible machine) */
		"mov $-1, %%ebx\n\t"
		"jp 4f\n\t"

"1:"
		/* Check feature test is supported */
		"mov $0, %%eax\n\t"
		"cpuid\n\t"
		"cmp $1, %%eax\n\t"
		"jge 2f\n\t"
		/* Unlucky - somehow cpuid 1 isn't supported */
		"mov $-2, %%ebx\n\t"
		"jp 4f\n\t"

"2:"
		/* Check if sse is supported (bit 25/26 in edx from cpuid 1) */
		"mov $1, %%eax\n\t"
		"cpuid\n\t"
		"mov $1, %%eax\n\t"
#if USE_SSE2
		"shl $26, %%eax\n\t"
#else
		"shl $25, %%eax\n\t"
#endif
		"test %%eax, %%edx\n\t"
		"jnz 3f\n\t"
		/* Not supported */
		"mov $0, %%ebx\n\t"
		"jp 4f\n\t"
"3:"
		/* Supported */
		"mov $1, %%ebx\n\t"
"4:"

			: "=b"(result) : : "%eax", "%ecx", "%edx");
#endif

	switch (result)
	{
	case -1:
		printf("Can't check for SSE - non pentium cpu\n");
		break;
	case -2:
		printf("No sse cpuid check available\n");
		break;
	case 0:
		/* No SSE support */
		break;
	case 1:
		/* SSE support */
		return 1;
	default:
		printf("Unknown return testing for SSE\n");
	}

	return 0;
}

int SSE_Supported(void)
{
	static int state = -1;

	if (state == -1)
		state = CheckSSE();

	return state;
}

#endif

#else

int SSE_Supported(void)
{
  return 0;
}

#endif



#if USE_SSE_VECTORIZE

#define DEBUG_SSE 0

#include "sse.h"

#include <string.h>

#ifdef USE_SSE2 
#include <emmintrin.h> 
#else
#include <xmmintrin.h> 
#endif

// #include <glib.h>
#include "sigmoid.h"


float *sse_malloc(size_t size)
{
  return (float *)_mm_malloc(size, ALIGN_SIZE);
}

void sse_free(float* ptr)
{
  _mm_free(ptr);
}

#if USE_SSE2
#include <stdint.h>

static const union {
	float f[4];
	__m128 ps;
} ones = {{1.0f, 1.0f, 1.0f, 1.0f}};

static const union {
	float f[4];
	__m128 ps;
} tens = {{10.0f, 10.0f, 10.0f, 10.0f}};

static const union {
	float f[4];
	__m128 ps;
} mmm ={{maxSigmoid,maxSigmoid,maxSigmoid,maxSigmoid}};

static const union {
	int32_t i32[4];
	__m128 ps;
} abs_mask = {{0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF}};

static inline __m128 sigmoid_positive_ps( __m128 xin )
{
  union {
    __m128i i;
    int32_t i32[4];
  } i;
  __m128 ex;
  float *ex_elem = (float*) &ex;
  __m128 mask10 = _mm_cmplt_ps( xin, tens.ps );

  __m128 x1 = _mm_mul_ps( xin, tens.ps );
  i.i  = _mm_cvttps_epi32( x1 );
  ex_elem[0] = e[i.i32[0]];
  ex_elem[1] = e[i.i32[1]];
  ex_elem[2] = e[i.i32[2]];
  ex_elem[3] = e[i.i32[3]];
  x1 = _mm_sub_ps( x1, _mm_cvtepi32_ps( i.i ) ); 
  x1 = _mm_add_ps( x1, tens.ps );
  x1 = _mm_mul_ps( x1, ex );

  x1 = _mm_or_ps( _mm_and_ps( mask10, x1 ) , _mm_andnot_ps ( mask10 , mmm.ps));
  
  x1 = _mm_add_ps( x1, ones.ps ); 
#ifdef __FAST_MATH__
  return _mm_rcp_ps( x1 );
#else
  return _mm_div_ps( ones.ps, x1 );
#endif
}

static inline __m128
sigmoid_ps( __m128 xin )
{	
  __m128 mask = _mm_cmplt_ps( xin, _mm_setzero_ps() );
  xin = _mm_and_ps (xin , abs_mask.ps ); /* Abs. value by clearing signbit */
  __m128 c = sigmoid_positive_ps(xin);
  return _mm_or_ps( _mm_and_ps( mask, c ) , _mm_andnot_ps ( mask , _mm_sub_ps( ones.ps, c )));
}

#endif  // USE_SSE2

static void
EvaluateSSE(const neuralnet *pnn, const float arInput[], float ar[], float arOutput[])
{
  const int cHidden = pnn->cHidden;
  int i, j;
  float* prWeight = pnn->arHiddenWeight;
  __m128 vec0, vec1, vec3, sum;

  int const misAligned = (pnn->cHidden & 0x3);
  // two extra simple checks for aligned case
  
  /* Calculate activity at hidden nodes */
  memcpy(ar, pnn->arHiddenThreshold, cHidden * sizeof(float));

  if( misAligned ) {
    SSE_ALIGN(float w[4]);
    for (i = 0; i < pnn->cInput; i++) {
      float const ari = arInput[i];

      if (!ari) {
	prWeight += cHidden;
      } else {
	float *pr = ar;
	if (ari == 1.0f) {
	  for( j = (cHidden >> 2); j; j--, pr += 4, prWeight += 4 ) {
	    vec0 = _mm_load_ps( pr );
	    memcpy(w, prWeight, sizeof(w));
	    vec1 = _mm_load_ps( w );  
	    sum =  _mm_add_ps(vec0, vec1);
	    _mm_store_ps(pr, sum);
	  }
	  for( j = misAligned; j; j-- ) {
	    *pr++ += *prWeight++;
	  }
	} else {
	  __m128 scalevec = _mm_set1_ps( ari );
	  for( j = (cHidden >> 2); j; j--, pr += 4, prWeight += 4 ) {
	    vec0 = _mm_load_ps( pr );
	    memcpy(w, prWeight, sizeof(w));
	    vec1 = _mm_load_ps( w );  
	    vec3 = _mm_mul_ps( vec1, scalevec );
	    sum =  _mm_add_ps( vec0, vec3 );
	    _mm_store_ps ( pr, sum );
	  }
	  for( j = misAligned; j; j-- ) {
	    *pr++ += *prWeight++ * ari;
	  }
	}
      }
    }
  } else {
    for (i = 0; i < pnn->cInput; i++) {
      float const ari = arInput[i];

      if (!ari) {
	prWeight += cHidden;
      } else {
	float *pr = ar;
	if (ari == 1.0f) {
	  for( j = (cHidden >> 2); j; j--, pr += 4, prWeight += 4 ) {
	    vec0 = _mm_load_ps( pr );  
	    vec1 = _mm_load_ps( prWeight );  
	    sum =  _mm_add_ps(vec0, vec1);
	    _mm_store_ps(pr, sum);
	  }
	} else {
	  __m128 scalevec = _mm_set1_ps( ari );
	  for( j = (cHidden >> 2); j; j--, pr += 4, prWeight += 4 ) {
	    vec0 = _mm_load_ps( pr );  
	    vec1 = _mm_load_ps( prWeight ); 
	    vec3 = _mm_mul_ps( vec1, scalevec );
	    sum =  _mm_add_ps( vec0, vec3 );
	    _mm_store_ps ( pr, sum );
	  }
	}
      }
    }
  }

#if USE_SSE2
    {
      __m128 scalevec = _mm_set1_ps(pnn->rBetaHidden);
      float *par = ar;
      for (i = (cHidden >> 2); i; i--, par += 4) {
	__m128 vec = _mm_load_ps(par);
	vec = _mm_mul_ps(vec, scalevec);
	vec = sigmoid_ps(vec);
	_mm_store_ps(par, vec);
      }
      for( i = misAligned; i ; --i, ++par ) {
	*par = sigmoid(-pnn->rBetaHidden * *par); 
      }
    }
#else
    for (i = 0; i < cHidden; i++)
      ar[i] = sigmoid(-pnn->rBetaHidden * ar[i]);
#endif
    
    /* Calculate activity at output nodes */
    prWeight = pnn->arOutputWeight;

  if( misAligned ) {
    for( i = 0; i < pnn->cOutput; i++ ) {
      SSE_ALIGN(float w[4]);

       float r;
       float *pr = ar;
       sum = _mm_setzero_ps();
       for( j = (cHidden >> 2); j ; j--, prWeight += 4, pr += 4 ){
         vec0 = _mm_load_ps( pr );           /* Four floats into vec0 */
	 memcpy(w, prWeight, sizeof(w));
	 vec1 = _mm_load_ps( w );  
         vec3 = _mm_mul_ps ( vec0, vec1 );   /* Multiply */
         sum = _mm_add_ps( sum, vec3 );  /* Add */
       }

       vec0 = _mm_shuffle_ps(sum, sum,_MM_SHUFFLE(2,3,0,1));
       vec1 = _mm_add_ps(sum, vec0);
       vec0 = _mm_shuffle_ps(vec1,vec1,_MM_SHUFFLE(1,1,3,3));
       sum = _mm_add_ps(vec1,vec0);
       _mm_store_ss(&r, sum); 

       for( j = (pnn->cHidden & 0x3); j; j-- ) {
	 r += *pr++ * *prWeight++;
       }
       
       arOutput[ i ] = sigmoid( -pnn->rBetaOutput * (r + pnn->arOutputThreshold[ i ]));
    }

  } else {
    for( i = 0; i < pnn->cOutput; i++ ) {

       float r;
       float *pr = ar;
       sum = _mm_setzero_ps();
       for( j = (cHidden >> 2); j ; j--, prWeight += 4, pr += 4 ){
         vec0 = _mm_load_ps( pr );           /* Four floats into vec0 */
         vec1 = _mm_load_ps( prWeight );     /* Four weights into vect1 */ 
         vec3 = _mm_mul_ps ( vec0, vec1 );   /* Multiply */
         sum = _mm_add_ps( sum, vec3 );  /* Add */
       }

       vec0 = _mm_shuffle_ps(sum, sum,_MM_SHUFFLE(2,3,0,1));
       vec1 = _mm_add_ps(sum, vec0);
       vec0 = _mm_shuffle_ps(vec1,vec1,_MM_SHUFFLE(1,1,3,3));
       sum = _mm_add_ps(vec1,vec0);
       _mm_store_ss(&r, sum); 

       arOutput[ i ] = sigmoid( -pnn->rBetaOutput * (r + pnn->arOutputThreshold[ i ]));
    }
  }
}

extern int
NeuralNetEvaluateSSE(const neuralnet *pnn, float arInput[], float arOutput[])
{
    float* ar = (float*) _mm_malloc(pnn->cHidden * sizeof(float), ALIGN_SIZE);
    if (!ar) return -1;

#if DEBUG_SSE
    assert(sse_aligned(ar));
  assert(sse_aligned(arInput));
#endif

    EvaluateSSE(pnn, arInput, ar, arOutput);
    _mm_free(ar);
    return 0;
}

#else

int NeuralNetEvaluateSSE(const neuralnet *pnn __attribute__((unused)), float arInput[] __attribute__((unused)), float arOutput[] __attribute__((unused))) {
  assert(0);
}

#endif

extern int fuseSSE;

int
useSSE(int use) {
  if( use ) {
    if( SSE_Supported() ) {
      fuseSSE = 1;
      return 1;
    }
    return 0;
  } else {
    fuseSSE = 0;
    return 1;
  }
}
  
