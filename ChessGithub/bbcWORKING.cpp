#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "nnue.cpp"




#ifdef _WIN32  //for both 32 and 64 bit versions
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

#define U64 unsigned long long

// FEN dedug positions
#define empty_board "8/8/8/8/8/8/8/8 b - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "





enum { //a8 is 0, a1 is 56, h1 is 63,e4 is 36, f5 is 29
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq //no_sq for enpassant
};

const char *square_to_coordinates[]={
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

//encode pieces 
//uppercase-white, lowercase-black
enum { P, N, B, R, Q, K, p, n, b, r, q, k };

//sides to move
enum {white, black, both}; //white=0 black=1

//bishop and rook
enum { rook,bishop };

/*castling bits binary rep
    bin       dec      
    0001      1       white king side
    0010      2       white queen side
    0100      4       black king side
    1000      8       black queen side

    eg 1111  both sides can castle both directions
    eg 1001  white king side, black queen side
*/

enum { wk=1, wq=2, bk=4, bq=8 }; //for castling


//ASCII pieces,
char ascii_pieces[12] = {'P','N','B','R','Q','K','p','n','b','r','q','k'};
//char ascii_pieces[12] = "PNBRQKpnbrqk";


//unicode pieces
const char *unicode_pieces[12] = {"♟","♞","♝","♜","♛","♚",
                            "♙","♘","♗","♖","♕","♔"};

// const char* unicode_pieces[13] = {
//     ". ", "♙ ", "♘ ", "♗ ", "♖ ", "♕ ", "♔ ", 
//     ". ", "♟ ", "♞ ", "♝ ", "♜ ", "♛ ", "♚ "
// };

//convert ASCII character pieces to encoded constants
int char_pieces[128];
//INVALID SYNTAX IN C++ HENCE JUST DECLARE HERE THEN INIT IN INIT_ALL FUNC
// int char_pieces[] = {
//     ['P']=P, ['N']=N, ['B']=B, ['R']=R, ['Q']=Q, ['K']=K,
//     ['p']=p, ['n']=n, ['b']=b, ['r']=r, ['q']=q, ['k']=k,
// };

//promoted pieces
char promoted_pieces[12];
// char promoted_pieces[]  = {   //lowercase regaardless of colour (for printmovelist ??)
//     [Q]='q', [R]='r',[B]='b', [N]='n',
//     [q]='q', [r]='r',[b]='b', [n]='n',
// };




//piece bitboards
U64 bitboards[12]; //global so init with 0

//occupancy bitboards
U64 occupancies[3];

//side to move;
int side;

//enpassant square
int enpassant = no_sq;

//castling rights
int castle; //be 0 by default

// "almost" unique position identifier aka hash key or position key
U64 hash_key = 0ULL; //0 by default since global var but okay no prob 






// --- Transposition Table Definitions ---            //by GEMINI
enum { hash_exact, hash_alpha, hash_beta };

typedef struct {
    U64 hash_key;
    int depth;
    int flag;
    int score;
    int best_move;
} tt_entry;

// ~16MB hash table (must be a power of 2 for fast bitwise ANDing)
const int hash_entries = 1048576; // 1024 * 1024
tt_entry *hash_table;

// Init and Clear functions
void init_hash_table() {
    hash_table = (tt_entry *)calloc(hash_entries, sizeof(tt_entry));
}

void clear_hash_table() {
    // calloc already zeros it, but we use this for the "ucinewgame" command
    memset(hash_table, 0, hash_entries * sizeof(tt_entry));
}









//half move counter
int ply;

// --- Add this array for NNUE ---       //NNUE ACCUMULATOR ARRAY
#define MAX_PLY 128
Accumulator acc_stack[MAX_PLY];

// Helper to get the 0-767 index for the neural net
inline int GetFeatureIndex(int piece, int square) {
    return piece * 64 + square;
}

//best move
int best_move;










//RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS
//RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS
//RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS
//RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS RANDOM NUMBERS

//pseudo random number state
unsigned int random_state = 1804289383;

//generate 32-bit pseudo legal numbers
unsigned int get_random_U32_number(){

	//get current state
	unsigned int number = random_state;

	//XOR shift algorithm
	number ^= number << 13;
	number ^= number >> 17;
	number ^= number << 5;

	//update random number state
	random_state = number;

	//return random number
	return number;
}


//geenrate 64-bit pseudo legal numbers
U64 get_random_U64_number(){

	//define 4 random numbers
	U64 n1,n2,n3,n4;
	//init random numbers slicing 16 bits from MS1B side
	n1 = (U64) ( get_random_U32_number() ) & 0xFFFF ;
	n2 = (U64) ( get_random_U32_number() ) & 0xFFFF ;
	n3 = (U64) ( get_random_U32_number() ) & 0xFFFF ;
	n4 = (U64) ( get_random_U32_number() ) & 0xFFFF ;

	//return random number
	return n1 | (n2<<16) | (n3<<32) | (n4<<48) ;
}

//generate magic number candidate
U64 generate_magic_number(){
	//bitwise and multiple times to reduce the number of set bits
	return  get_random_U64_number() &  get_random_U64_number() &  get_random_U64_number();
}







//BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION
//BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION
//BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION
//BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION BIT MANIPULATION


//set/get/pop macros
//square uses enum value for readability
#define set_bit(bitboard,square) (bitboard |= (1ULL << square)) 
#define get_bit(bitboard,square) (bitboard & (1ULL << square))
#define pop_bit(bitboard,square) (get_bit(bitboard,square) ? bitboard ^= (1ULL << square) : 0)


//count bits within a bitboard (Brian Kernighan's algorithm)
static inline int count_bits(U64 bitboard){

    int count = 0;  //bit counter

    //consecutively reset least significant first bit
    while(bitboard){
        
        count++; //increment count

        //reset least significant first bit
        bitboard &= (bitboard - 1); //BLACK MAGIC
    }

    return count;
}


//get least significant first bit index
static inline int get_ls1b_index(U64 bitboard){

    //make sure bitboard is not zero
    if(bitboard){
        //count trailing bits before LS1B
        return count_bits( (bitboard & -bitboard) - 1 );
    }
    else{
        return -1;
    }

}





/**********************************\
 ==================================
 
            Zobrist keys
 
 ==================================
\**********************************/

// random piece keys [piece][square]
U64 piece_keys[12][64];

// random enpassant keys [square]
U64 enpassant_keys[64];

// random castling keys
U64 castle_keys[16];

// random side key
U64 side_key;

// init random hash keys
void init_random_keys()
{
    // update pseudo random number state
    random_state = 1804289383;

    // loop over piece codes
    for (int piece = P; piece <= k; piece++)
    {
        // loop over board squares
        for (int square = 0; square < 64; square++)
            // init random piece keys
            piece_keys[piece][square] = get_random_U64_number();
    }
    
    // loop over board squares
    for (int square = 0; square < 64; square++)
        // init random enpassant keys
        enpassant_keys[square] = get_random_U64_number();
    
    // loop over castling keys
    for (int index = 0; index < 16; index++)
        // init castling keys
        castle_keys[index] = get_random_U64_number();
        
    // init random side key
    side_key = get_random_U64_number();
}


// generate "almost" unique position ID aka hash key from scratch
U64 generate_hash_key()
{
    // final hash key
    U64 final_key = 0ULL;
    
    // temp piece bitboard copy
    U64 bitboard;
    
    // loop over piece bitboards
    for (int piece = P; piece <= k; piece++)
    {
        // init piece bitboard copy
        bitboard = bitboards[piece];
        
        // loop over the pieces within a bitboard
        while (bitboard)
        {
            // init square occupied by the piece
            int square = get_ls1b_index(bitboard);
            
            // hash piece
            final_key ^= piece_keys[piece][square];
            
            // pop LS1B
            pop_bit(bitboard, square);
        }
    }
    
    // if enpassant square is on board
    if (enpassant != no_sq)
        // hash enpassant
        final_key ^= enpassant_keys[enpassant];
    
    // hash castling rights
    final_key ^= castle_keys[castle];
    
    // hash the side only if black is to move
    if (side == black) final_key ^= side_key;
    
    // return generated hash key
    return final_key;
}




//INPUT OUTPUT INPUT OUTPUT INPUT OUTPUT ???????
//INPUT OUTPUT INPUT OUTPUT INPUT OUTPUT ???????
//INPUT OUTPUT INPUT OUTPUT INPUT OUTPUT ???????
//INPUT OUTPUT INPUT OUTPUT INPUT OUTPUT ???????


//print bitboard
void print_bitboard(U64 bitboard){

    printf("\n");
    //loop over board ranks
    for(int rank=0;rank<8;rank++){
        for(int file=0;file<8;file++){
            //convert to sqaure index
            int square = rank*8 + file;

            //print ranks
            if(file==0) printf(" %d  ",8-rank);


            //print bit-state (either 1 or 0)
            printf(" %d",  get_bit(bitboard,square) ? 1:0  );
            // << square   -  shifts that single bit to the left by the
                        //number of the value of square
            // 1ULL means 1 represented as a 64 bit number
            //only one 1 out of 64 bits. effectively choosing the position based on value of square

            //if the bit at that square is 1 the result is non-zero (true)
            //if bit is 0 then 0 (false)

        }
        printf("\n");
    }
    printf("\n     a b c d e f g h\n\n");

    //print bitboard as unsigned decimal number
    printf("       Bitboard: %llu\n\n",bitboard);
}


//print board
void print_board(){

    printf("\n");

    for(int rank=0;rank<8;rank++){
        for(int file=0;file<8;file++){

            int square = rank*8+file;

            //print ranks
            if(file==0) printf(" %d  ",8-rank);

            //define piece variable
            int piece = -1;

            //loop over all piece bitboards
            for(int bb_piece = P; bb_piece<=k; bb_piece++){

                if(get_bit(bitboards[bb_piece],square))
                    piece = bb_piece;
            }

            //print different piece set depending on OS
            #ifdef _WIN64
                printf(" %c",(piece==-1) ? '.' : ascii_pieces[piece]);
            #else
                printf(" %s",(piece==-1) ? "." : unicode_pieces[piece]);
            #endif 

        }
        printf("\n");
    }

    //print board files
    printf("\n     a b c d e f g h\n\n");

    //print side to move
    printf("    Side to move: %s\n", (side==0) ? "white" : "black" );

    //print enpassant square
    printf("    En passant:  %s\n", 
        (enpassant != no_sq) ? square_to_coordinates[enpassant] : "no_sq");

    //print castling rights
    printf("    Castling:  %c%c%c%c\n", 
        (castle & wk) ? 'K' : '-',
        (castle & wq) ? 'Q' : '-',
        (castle & bk) ? 'k' : '-',
        (castle & bq) ? 'q' : '-'
    );


     // print hash key
    printf("     Hash key:  %llx\n\n", hash_key);


}



// parse FEN string   (directly copy pasted)
void parse_fen(const char *fen)
{
    // reset board position (bitboards)
    memset(bitboards, 0ULL, sizeof(bitboards));
    
    // reset occupancies (bitboards)
    memset(occupancies, 0ULL, sizeof(occupancies));
    
    // reset game state variables
    side = 0;
    enpassant = no_sq;
    castle = 0;
    
    // loop over board ranks
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            // init current square
            int square = rank * 8 + file;
            
            // match ascii pieces within FEN string
            if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
            {
                // init piece type
                int piece = char_pieces[*fen];
                
                // set piece on corresponding bitboard
                set_bit(bitboards[piece], square);
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match empty square numbers within FEN string
            if (*fen >= '0' && *fen <= '9')
            {
                // init offset (convert char 0 to int 0)
                int offset = *fen - '0';
                
                // define piece variable
                int piece = -1;
                
                // loop over all piece bitboards
                for (int bb_piece = P; bb_piece <= k; bb_piece++)
                {
                    // if there is a piece on current square
                    if (get_bit(bitboards[bb_piece], square))
                        // get piece code
                        piece = bb_piece;
                }
                
                // on empty current square
                if (piece == -1)
                    // decrement file
                    file--;
                
                // adjust file counter
                file += offset;
                
                // increment pointer to FEN string
                fen++;
            }
            
            // match rank separator
            if (*fen == '/')
                // increment pointer to FEN string
                fen++;
        }
    }
    
    // got to parsing side to move (increment pointer to FEN string)
    fen++;
    
    // parse side to move
    //(*fen == 'w') ? (side = white) : (side = black);
    side = (*fen == 'w') ? white : black;

    // go to parsing castling rights
    fen += 2;
    
    // parse castling rights
    while (*fen != ' ')
    {
        switch (*fen)
        {
            case 'K': castle |= wk; break;
            case 'Q': castle |= wq; break;
            case 'k': castle |= bk; break;
            case 'q': castle |= bq; break;
            case '-': break;
        }

        // increment pointer to FEN string
        fen++;
    }
    
    // got to parsing enpassant square (increment pointer to FEN string)
    fen++;
    
    // parse enpassant square
    if (*fen != '-')
    {
        // parse enpassant file & rank
        int file = fen[0] - 'a';
        int rank = 8 - (fen[1] - '0');
        
        // init enpassant square
        enpassant = rank * 8 + file;
    }
    
    // no enpassant square
    else
        enpassant = no_sq;
    
    // loop over white pieces bitboards
    for (int piece = P; piece <= K; piece++)
        // populate white occupancy bitboard
        occupancies[white] |= bitboards[piece];
    
    // loop over black pieces bitboards
    for (int piece = p; piece <= k; piece++)
        // populate white occupancy bitboard
        occupancies[black] |= bitboards[piece];
    
    // init all occupancies
    occupancies[both] |= occupancies[white];
    occupancies[both] |= occupancies[black];


    // init hash key
    hash_key = generate_hash_key();

}












//ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS
//ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS
//ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS
//ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS ATTACKS


//     not_A_file
//
//  8   0 1 1 1 1 1 1 1
//  7   0 1 1 1 1 1 1 1
//  6   0 1 1 1 1 1 1 1
//  5   0 1 1 1 1 1 1 1
//  4   0 1 1 1 1 1 1 1
//  3   0 1 1 1 1 1 1 1
//  2   0 1 1 1 1 1 1 1
//  1   0 1 1 1 1 1 1 1

//      a b c d e f g h

//not_X_file constants (above similar tables as integer)
const U64 not_A_file = 18374403900871474942ULL; //ULL to make it unsigned long long
const U64 not_B_file = 18302063728033398269ULL;
const U64 not_C_file = 18157383382357244923ULL; //C,D,E,F unused
const U64 not_D_file = 17868022691004938231ULL;
const U64 not_E_file = 17289301308300324847ULL;
const U64 not_F_file = 16131858542891098079ULL;
const U64 not_G_file = 13816973012072644543ULL;
const U64 not_H_file = 9187201950435737471ULL;

const U64 not_AB_file = 18229723555195321596ULL; //for knight
const U64 not_GH_file = 4557430888798830399ULL;

//GENERATED USING THIS HELPER FUNC. CHANGE VALUE OF FILE AS NEEDED
// U64 not_X_file=0ULL;
//     for(int rank=0;rank<8;rank++){
//         for(int file=0;file<8;file++){
//
//             int square = rank*8 + file;
//             if(file!=7) set_bit(not_X_file,square);
//
//         }
//     }
//     print_bitboard(not_X_file);


//relevant occupancy bit count for every square on board
const int bishop_relevant_bits[64] = {
	6, 5, 5, 5, 5, 5, 5, 6,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 5, 5, 5, 5, 5, 5, 6
};

const int rook_relevant_bits[64] = {
	12, 11, 11, 11, 11, 11, 11, 12,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	12, 11, 11, 11, 11, 11, 11, 12
};


//GENERATED USING THIS FORMAT
//  for(int rank=0;rank<8;rank++){
// 		for(int file=0;file<8;file++){

// 			int square = rank*8 + file;

// 			printf( "%d, ", count_bits(mask_bishop_attacks(square)));

// 		}
// 		printf("\n");
 
// 	}



//MAGIC NUMS ARRAYS 
//(values inserted using init_magic_numbers() function)
//NOT ANYMORE. NOW HARDCODED

//rook magic numbers
U64 rook_magic_numbers[64] = { //one for each square
    0x8a80104000800020ULL ,
    0x140002000100040ULL ,
    0x2801880a0017001ULL ,
    0x100081001000420ULL ,
    0x200020010080420ULL ,
    0x3001c0002010008ULL ,
    0x8480008002000100ULL ,
    0x2080088004402900ULL ,
    0x800098204000ULL ,
    0x2024401000200040ULL ,
    0x100802000801000ULL ,
    0x120800800801000ULL ,
    0x208808088000400ULL ,
    0x2802200800400ULL ,
    0x2200800100020080ULL ,
    0x801000060821100ULL ,
    0x80044006422000ULL ,
    0x100808020004000ULL ,
    0x12108a0010204200ULL ,
    0x140848010000802ULL ,
    0x481828014002800ULL ,
    0x8094004002004100ULL ,
    0x4010040010010802ULL ,
    0x20008806104ULL ,
    0x100400080208000ULL ,
    0x2040002120081000ULL ,
    0x21200680100081ULL ,
    0x20100080080080ULL ,
    0x2000a00200410ULL ,
    0x20080800400ULL ,
    0x80088400100102ULL ,
    0x80004600042881ULL ,
    0x4040008040800020ULL ,
    0x440003000200801ULL ,
    0x4200011004500ULL ,
    0x188020010100100ULL ,
    0x14800401802800ULL ,
    0x2080040080800200ULL ,
    0x124080204001001ULL ,
    0x200046502000484ULL ,
    0x480400080088020ULL ,
    0x1000422010034000ULL ,
    0x30200100110040ULL ,
    0x100021010009ULL ,
    0x2002080100110004ULL ,
    0x202008004008002ULL ,
    0x20020004010100ULL ,
    0x2048440040820001ULL ,
    0x101002200408200ULL ,
    0x40802000401080ULL ,
    0x4008142004410100ULL ,
    0x2060820c0120200ULL ,
    0x1001004080100ULL ,
    0x20c020080040080ULL ,
    0x2935610830022400ULL ,
    0x44440041009200ULL ,
    0x280001040802101ULL ,
    0x2100190040002085ULL ,
    0x80c0084100102001ULL ,
    0x4024081001000421ULL ,
    0x20030a0244872ULL ,
    0x12001008414402ULL ,
    0x2006104900a0804ULL ,
    0x1004081002402ULL 
};

//bishop magic numbers
U64 bishop_magic_numbers[64] = {
    0x40040844404084ULL ,
    0x2004208a004208ULL ,
    0x10190041080202ULL ,
    0x108060845042010ULL ,
    0x581104180800210ULL ,
    0x2112080446200010ULL ,
    0x1080820820060210ULL ,
    0x3c0808410220200ULL ,
    0x4050404440404ULL ,
    0x21001420088ULL ,
    0x24d0080801082102ULL ,
    0x1020a0a020400ULL ,
    0x40308200402ULL ,
    0x4011002100800ULL ,
    0x401484104104005ULL ,
    0x801010402020200ULL ,
    0x400210c3880100ULL ,
    0x404022024108200ULL ,
    0x810018200204102ULL ,
    0x4002801a02003ULL ,
    0x85040820080400ULL ,
    0x810102c808880400ULL ,
    0xe900410884800ULL ,
    0x8002020480840102ULL ,
    0x220200865090201ULL ,
    0x2010100a02021202ULL ,
    0x152048408022401ULL ,
    0x20080002081110ULL ,
    0x4001001021004000ULL ,
    0x800040400a011002ULL ,
    0xe4004081011002ULL ,
    0x1c004001012080ULL ,
    0x8004200962a00220ULL ,
    0x8422100208500202ULL ,
    0x2000402200300c08ULL ,
    0x8646020080080080ULL ,
    0x80020a0200100808ULL ,
    0x2010004880111000ULL ,
    0x623000a080011400ULL ,
    0x42008c0340209202ULL ,
    0x209188240001000ULL ,
    0x400408a884001800ULL ,
    0x110400a6080400ULL ,
    0x1840060a44020800ULL ,
    0x90080104000041ULL ,
    0x201011000808101ULL ,
    0x1a2208080504f080ULL ,
    0x8012020600211212ULL ,
    0x500861011240000ULL ,
    0x180806108200800ULL ,
    0x4000020e01040044ULL ,
    0x300000261044000aULL ,
    0x802241102020002ULL ,
    0x20906061210001ULL ,
    0x5a84841004010310ULL ,
    0x4010801011c04ULL ,
    0xa010109502200ULL ,
    0x4a02012000ULL ,
    0x500201010098b028ULL ,
    0x8040002811040900ULL ,
    0x28000010020204ULL ,
    0x6000020202d0240ULL ,
    0x8918844842082200ULL ,
    0x4010011029020020ULL 
};




//pawn attacks table [side][square]
U64 pawn_attacks[2][64];

//knight attacks table [square] 
//since white,black knights both have same movement (unlike pawns)
U64 knight_attacks[64];

//king attacks table [square]
U64 king_attacks[64];



//bishop attacks masks
U64 bishop_masks[64];

//rook attacks masks
U64 rook_masks[64];

//bishop attacks table [square][occupancies]
U64 bishop_attacks[64][512];

//rook attacks table [square][occupancies]
U64 rook_attacks[64][4096];    //CHANGED TO 4096 (EARLIER 512)                //WHY NOT 4096







//generate pawn attacks
U64 mask_pawn_attacks(int side,int square){

    //result attacks bitboard
    U64 attacks = 0ULL;
    
    //piece bitboard
    U64 bitboard = 0ULL;

    //set piece on board
    set_bit(bitboard,square);

    //print_bitboard(bitboard);

    //white pawns
    if(side==0){//white to play
        //rightshift for white

        //IF OUT OF BOUNDS THEN 0
        //DONE BY COMPILER??????????????????????????????

        if( (bitboard >> 7) & not_A_file) //A
            attacks |= (bitboard >> 7);
        if( (bitboard >> 9) & not_H_file) //H
            attacks |= (bitboard >> 9);
        //look at enum definition
        //e4 is 36, f5 is 29
        //hence rightshift for white
    }
    else{//else black to play
        //leftshift for black
        //NOT h file ka hai. think carefully
        //h pe hai then 7 valid means not on h then 9 valid

        //bitwise and of not_H_file and bitboard<<7
        //will be 0 if bitboard<<7 lies on H file
        //and in this case if-condition will not execute
        if( (bitboard << 7) & not_H_file) //H
            attacks |= (bitboard << 7);
        if( (bitboard << 9) & not_A_file) //A
            attacks |= (bitboard << 9);
        
    }

    //return attack map
    return attacks;
}

//generate knight attacks
U64 mask_knight_attacks(int square){

    //result attacks bitboard
    U64 attacks = 0ULL;
    
    //piece bitboard
    U64 bitboard = 0ULL;

    //set piece on board
    set_bit(bitboard,square);

    //print_bitboard(bitboard);

    //IF OUT OF BOUNDS THEN 0
    //DONE BY COMPILER??????????????????????????????

    //just need to handle the wrap-around logic so it doesnt wrap around
    if( (bitboard >> 17) & not_H_file) //if piece on A then attack will go to H
        attacks |= (bitboard >> 17);   //bitwise anding it with not_H removes the bit that was wrongly on H
    if( (bitboard >> 15) & not_A_file)
        attacks |= (bitboard >> 15);
    if( (bitboard >> 10) & not_GH_file)
        attacks |= (bitboard >> 10);
    if( (bitboard >> 6) & not_AB_file)
        attacks |= (bitboard >> 6);

    if( (bitboard << 17) & not_A_file)
        attacks |= (bitboard << 17);
    if( (bitboard << 15) & not_H_file)
        attacks |= (bitboard << 15);
    if( (bitboard << 10) & not_AB_file)
        attacks |= (bitboard << 10);
    if( (bitboard << 6) & not_GH_file)
        attacks |= (bitboard << 6);
    
    
    //return attack map
    return attacks;
}


//generate king attacks
U64 mask_king_attacks(int square){

    //result attacks bitboard
    U64 attacks = 0ULL;
    
    //piece bitboard
    U64 bitboard = 0ULL;

    //set piece on board
    set_bit(bitboard,square);


    //print_bitboard(bitboard);


    //IF OUT OF BOUNDS THEN 0
    //DONE BY COMPILER??????????????????????????????

    //just need to handle the wrap-around logic so it doesnt wrap around
    
    //( - rightshift >>, + leftshift <<  )
    //notice dir of bitshift. going up minus, going down plus (mostly)
    //left minus, right plus (if up/down unchanged)

    // - - -
    // -   +
    // + + +

    if( (bitboard >> 8) ) //up from A
        attacks |= (bitboard >> 8);
    if( (bitboard >> 9) & not_H_file) //diag up left from A
        attacks |= (bitboard >> 9);
    if( (bitboard >> 7) & not_A_file) //diag up right from H
        attacks |= (bitboard >> 7);

    if( (bitboard >> 1) & not_H_file) //left from A (wrongly to H)
        attacks |= (bitboard >> 1);


    if( (bitboard << 1) & not_A_file) //right from H
        attacks |= (bitboard << 1);

    if( (bitboard << 8) ) //down from A
        attacks |= (bitboard << 8);
    if( (bitboard << 7) & not_H_file) //diag down left from A
        attacks |= (bitboard << 7);
    if( (bitboard << 9) & not_A_file)//diag down right from H
        attacks |= (bitboard << 9);
   
    
    //return attack map
    return attacks;
}


//mask bishop attacks
U64 mask_bishop_attacks(int square){

    //result attacks bitboard
    U64 attacks = 0ULL;

    //init ranks and files
    int r,f;

    //init target rank and file
    int tr = square / 8;
    int tf = square % 8;

    //mask relevant bishop occupancy bits

    //right
    for(r=tr+1,f=tf+1  ;  r<=6 && f<=6  ;   r++,f++ ){
        attacks |= (1ULL << (r*8 + f));
    }
    for(r=tr-1,f=tf+1  ;  r>=1 && f<=6  ;   r--,f++ ){
        attacks |= (1ULL << (r*8 + f));
    }
    //left
    for(r=tr+1,f=tf-1  ;  r<=6 && f>=1  ;   r++,f-- ){
        attacks |= (1ULL << (r*8 + f));
    }
    for(r=tr-1,f=tf-1  ;  r>=1 && f>=1  ;   r--,f-- ){
        attacks |= (1ULL << (r*8 + f));
    }
    

    return attacks;
}

//mask rook attacks
U64 mask_rook_attacks(int square){

    //result attacks bitboard
    U64 attacks = 0ULL;

    //init ranks and files
    int r,f;

    //init target rank and file
    int tr = square / 8;
    int tf = square % 8;

    //mask relevant rook occupancy bits

    //rank changes
    for(r=tr+1 ; r<=6 ; r++){
        attacks |= (1ULL << (r*8 + tf)); //tf since only rank changes
    }
    for(r=tr-1 ; r>=1 ; r--){
        attacks |= (1ULL << (r*8 + tf)); //tf since file const
    }

    //file changes
    for(f=tf+1 ; f<=6 ; f++){
        attacks |= (1ULL << (tr*8 + f)); //tr since rank const
    }
    for(f=tf-1 ; f>=1 ; f--){
        attacks |= (1ULL << (tr*8 + f)); //tr since rank const
    }
    

    return attacks;
}



//generate bishop attacks on the fly
U64 bishop_attacks_on_the_fly(int square,U64 block){

    //result attacks bitboard
    U64 attacks = 0ULL;

    //init ranks and files
    int r,f;

    //init target rank and file
    int tr = square / 8;
    int tf = square % 8;

    //generate bishop attacks

    //right               /*6 to 7, 1 to 0*/
    for(r=tr+1,f=tf+1  ;  r<=7 && f<=7  ;  r++,f++ ){
        attacks |= (1ULL << (r*8 + f));
        if( (1ULL << (r*8 + f) & block ) ) break;
    }
    for(r=tr-1,f=tf+1  ;  r>=0 && f<=7  ;  r--,f++ ){
        attacks |= (1ULL << (r*8 + f));
        if( (1ULL << (r*8 + f) & block ) ) break;
    }
    //left
    for(r=tr+1,f=tf-1  ;  r<=7 && f>=0  ;  r++,f-- ){
        attacks |= (1ULL << (r*8 + f));
        if( (1ULL << (r*8 + f) & block ) ) break;
    }
    for(r=tr-1,f=tf-1  ;  r>=0 && f>=0  ;  r--,f-- ){
        attacks |= (1ULL << (r*8 + f));
        if( (1ULL << (r*8 + f) & block ) ) break;
    }
    

    return attacks;
}

//generate rook attacks on the fly
U64 rook_attacks_on_the_fly(int square,U64 block){

    //result attacks bitboard
    U64 attacks = 0ULL;

    //init ranks and files
    int r,f;

    //init target rank and file
    int tr = square / 8;
    int tf = square % 8;

    //mask relevant rook occupancy bits

    //rank changes      /*6 to 7, 1 to 0*/
    for(r=tr+1 ; r<=7 ; r++){
        attacks |= (1ULL << (r*8 + tf)); //tf since only rank changes
        if( (1ULL << (r*8 + tf)) & block ) break;
    }
    for(r=tr-1 ; r>=0 ; r--){
        attacks |= (1ULL << (r*8 + tf)); //tf since file const
        if( (1ULL << (r*8 + tf)) & block ) break;
    }

    //file changes
    for(f=tf+1 ; f<=7 ; f++){
        attacks |= (1ULL << (tr*8 + f)); //tr since rank const
        if( (1ULL << (tr*8 + f)) & block ) break;
    }
    for(f=tf-1 ; f>=0 ; f--){
        attacks |= (1ULL << (tr*8 + f)); //tr since rank const
        if( (1ULL << (tr*8 + f)) & block ) break;
    }
    

    return attacks;
}





//init leaper pieces attacks
void init_leapers_attacks(){

    //loop over 64 squares
    for(int square =0;square<64;square++){

        //init pawn attacks
        pawn_attacks[white][square] = mask_pawn_attacks(white,square);
        pawn_attacks[black][square] = mask_pawn_attacks(black,square);

        //init knight attacks
        knight_attacks[square] = mask_knight_attacks(square);

        //init king attack
        king_attacks[square] = mask_king_attacks(square);

    }


}



//set occupancies (VISIT AGAIN SINCE KINDA UNINTUITIVE AT FIRST)
U64 set_occupancy(int index,int bits_in_mask, U64 attack_mask ){

    //occupancy map
    U64 occupancy = 0ULL;

    //loop over the range of bits within attack mask
    for(int count=0; count<bits_in_mask;count++){

        //get LS1B index of attacks mask
        int square = get_ls1b_index(attack_mask);

        //pop LS1B in attack map
        pop_bit(attack_mask, square);

        //make sure occupancy is on board
        if(index & (1ULL<<count)){ //1ULL to make it 64 bits,else would have been 32 bit by default
            //populate occupancy map
            occupancy |= (1ULL << square);
        }

    }
    //return occupancy map
    return occupancy;
}



//MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS
//MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS
//MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS
//MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS MAGICS

//find appropriate magic number
U64 find_magic_number(int square ,int relevant_bits, int bishop){

	//init occupancies
	U64 occupancies[4096];

	//init attack tables
	U64 attacks[4096];

	//init used attacks
	U64 used_attacks[4096];

	//init attack mask for current piece
	U64 attack_mask = bishop ? 
		mask_bishop_attacks(square) : mask_rook_attacks(square);

	//init occupancy indices
	int occupancy_indices = 1 << relevant_bits; //n -> 2^n possible combinations

	//loop over occupancy iindices
	for(int index=0; index<occupancy_indices;index++){

		//init occupancies
		occupancies[index] = set_occupancy(index, relevant_bits, attack_mask);

		//init attacks
		attacks[index] = bishop ? 
			bishop_attacks_on_the_fly(square,occupancies[index]) :
				rook_attacks_on_the_fly(square,occupancies[index]);
				
	}

	//test magic numbers loop      //one mill not enough
                                //so changed to 100 million
	for(int random_count=0;random_count<1000000000;random_count++){

		//generate magic number candidate
		U64 magic_number = generate_magic_number();

		//skip inappropriate magic numbers
		if(count_bits( (attack_mask * magic_number) & 0xFF00000000000000 ) < 6)
			 continue;

		//init used attacks
		memset(used_attacks, 0ULL, sizeof(used_attacks));	//string.h function

		//init index and fail flag
		int index, fail;

		//test magic index loop
		for( index=0,fail=0; !fail && index<occupancy_indices; index++){

			//init magic index
			int magic_index = (int) ((occupancies[index] * magic_number) >> (64 - relevant_bits));

			//if magic index works
			if(used_attacks[magic_index] == 0ULL){
				//init used attacks
				used_attacks[magic_index] = attacks[index];
			}
			//otherwise
			else if(used_attacks[magic_index] != attacks[index]){
				//magic index doesnt work
				fail=1;
			}

		}

		//if magic number works return it
		if( !fail ) return magic_number;

	}

	//if magic number doesnt work
	printf(" magic number fails");
	return 0ULL;

}

//init magic numbers
void init_magic_numbers(){

	//loop over 64 board squares
//    printf("ROOK MAGIC\n");
	for(int square=0; square<64;square++){

		//init rook magic numbers                          //array declared at top
//		printf(" 0x%llxULL\n", find_magic_number(square, rook_relevant_bits[square],rook));
        
        //insert into array
        rook_magic_numbers[square]=find_magic_number(square, rook_relevant_bits[square],rook);
	
        //dont call the function in both print and array inserting func
        //even if doing so for testing
        //as it will mess up the global random_state
    
    }
//    printf("\n");

	//loop over 64 board squares
//    printf("BISHOP MAGIC\n");
	for(int square=0; square<64;square++){

		//init bishop magic numbers                          //array declared at top
//		printf(" 0x%llxULL\n", find_magic_number(square, bishop_relevant_bits[square],bishop));

        //insert into array
        bishop_magic_numbers[square]=find_magic_number(square, bishop_relevant_bits[square],bishop);
	
        //dont call the function in both print and array inserting func
        //even if doing so for testing
        //as it will mess up the global random_state
    }
//    printf("\n");

}


//init slider pieces' attack tables
void init_sliders_attacks(int bishop){

    //loop over 64 board squares
    for(int square =0;square<64;square++){

        //init bishop and rook masks
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);

        //init current mask
        U64 attack_mask = bishop ? 
            bishop_masks[square] : rook_masks[square];

        //init relevant occupancy bit count
        int relevant_bits_count = count_bits (attack_mask);

        //init occupancy indices
        int occupancy_indices = (1 << relevant_bits_count);

        //loop over occupancy indices
        for(int index=0;index<occupancy_indices;index++){

            if(bishop){   //bishop
                //init current occupancy variation
                U64 occupancy = 
                    set_occupancy(index,relevant_bits_count,attack_mask);
            
                //init magic index
                int magic_index = 
                    (occupancy * bishop_magic_numbers[square]) >> (64-bishop_relevant_bits[square]);

                //init bishop attacks
                bishop_attacks[square][magic_index] =
                    bishop_attacks_on_the_fly(square,occupancy);

            }
            else{   //rook
                //init current occupancy variation
                U64 occupancy = 
                    set_occupancy(index,relevant_bits_count,attack_mask);
            
                //init magic index
                int magic_index = 
                    (occupancy * rook_magic_numbers[square]) >> (64-rook_relevant_bits[square]);

                //init rook attacks
                rook_attacks[square][magic_index] =
                    rook_attacks_on_the_fly(square,occupancy);

            }



        }

    }
    


}

//get bishop attacks
static inline U64 get_bishop_attacks(int square,U64 occupancy){

    //get bishop attacks assuming current board occupancy
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits[square];

    //occupancy is the required hashed index?????????

    return bishop_attacks[square][occupancy];

}

//get bishop attacks
static inline U64 get_rook_attacks(int square,U64 occupancy){

    //get rook attacks assuming current board occupancy
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits[square];

    return rook_attacks[square][occupancy];

}

//get queen attacks
static inline U64 get_queen_attacks(int square,U64 occupancy){

    //init result attacks bitboard
    U64 queen_attacks = 0ULL;

    //init bishop occupancies
    U64 bishop_occupancy = occupancy;
    //init rook occupancies
    U64 rook_occupancy = occupancy;

    //get bishop attacks assuming current board occupancy
    bishop_occupancy &= bishop_masks[square];
    bishop_occupancy *= bishop_magic_numbers[square];
    bishop_occupancy >>= 64 - bishop_relevant_bits[square];

    //get bishop attacks
    queen_attacks = bishop_attacks[square][bishop_occupancy];

    //get rook attacks assuming current board occupancy
    rook_occupancy &= rook_masks[square];
    rook_occupancy *= rook_magic_numbers[square];
    rook_occupancy >>= 64 - rook_relevant_bits[square];
 
    //get rook attacks
    queen_attacks |= rook_attacks[square][rook_occupancy];
    //(bitwise ORed with previous(bishop) result)

    //return queen attacks
    return queen_attacks;
}





//MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR
//MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR
//MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR
//MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR MOVE GENERATOR



//is current given square attacked BY the current given side?
static inline int is_square_attacked(int square,int side){

    //attack by white pawns
    if( (side==white) && 
        (pawn_attacks[black][square] & bitboards[P]) )
        return 1;
    //attack by black pawns
    if( (side==black) && 
        (pawn_attacks[white][square] & bitboards[p]) )
        return 1;
    
    //attacked by knights
    if(knight_attacks[square] & 
        ((side==white) ? bitboards[N]:bitboards[n]) )
        return 1;

    //attacked by king
    if(king_attacks[square] & 
        ((side==white) ? bitboards[K]:bitboards[k]) )
        return 1;

    
    //attacked by bishops
    if(get_bishop_attacks(square,occupancies[both])
        & ((side==white) ? bitboards[B]:bitboards[b]))
        return 1;
    //attacked by rooks
    if(get_rook_attacks(square,occupancies[both])
        & ((side==white) ? bitboards[R]:bitboards[r]))
        return 1;
    //attacked by queen
    if(get_queen_attacks(square,occupancies[both])
        & ((side==white) ? bitboards[Q]:bitboards[q]))
        return 1;



    //by deffault return false
    return 0;
}

//print attacked squares
void print_attacked_squares(int side){

    printf("\n");
    //loop over board ranks
    for(int rank=0;rank<8;rank++){
        for(int file=0;file<8;file++){
            //convert to sqaure index
            int square = rank*8 + file;

            //print ranks
            if(file==0) printf(" %d  ",8-rank);

            //check whether current square is attacked or not
            printf("%d ",is_square_attacked(square,side)?1:0);


        }
        printf("\n");
    }
    printf("\n     a b c d e f g h\n\n");

    //print bitboard as unsigned decimal number
    //printf("       Bitboard: %llu\n\n",bitboard);

}




/*
           binary move bits                            hex consts

    0000 0000 0000 0000 0011 1111   source square        0x3f
    0000 0000 0000 1111 1100 0000   target square        0xfc0
    0000 0000 1111 0000 0000 0000   piece                0xf000
    0000 1111 0000 0000 0000 0000   promoted piece       0xf0000
    0001 0000 0000 0000 0000 0000   capture flag         0x100000
    0010 0000 0000 0000 0000 0000   double push flag     0x200000
    0100 0000 0000 0000 0000 0000   enpassant flag       0x400000
    1000 0000 0000 0000 0000 0000   castling flag        0x800000

*/

//encode move
#define encode_move(source, target, piece, promoted, capture, doublep, enpassant, castling) \
    (source) | (target << 6) |      \
    (piece << 12) | (promoted << 16)   \
    | (capture << 20) | (doublep << 21) |   \
    (enpassant << 22) | (castling << 23)

//extract source square
#define get_move_source(move) (move & 0x3f)

//extract target ssquare
#define get_move_target(move) ((move & 0xfc0) >> 6)

//extract piece
#define get_move_piece(move) ((move & 0xf000) >> 12)

//extract promoted piece
#define get_move_prom(move) ((move & 0xf0000) >> 16)

//extract capture flag
#define get_move_captureflag(move) ((move & 0x100000) )

//extract double pawn push flag
#define get_move_doublepflag(move) ((move & 0x200000) )

//extract enpasssant flag
#define get_move_enpassantflag(move) ((move & 0x400000) )

//extract castling flag
#define get_move_castlingflag(move) ((move & 0x800000) )


//move list structure
typedef struct{
    int moves[256];   //moves
    int count;        //move count
} moves;


//add move to the move list
static inline void add_move(moves * move_list, int move){

    move_list->moves[move_list->count] = move;

    //increment move count
    move_list->count++;

}



//print move (for UCI purpose)
void print_move(int move){

    // printf("%s%s%c\n", square_to_coordinates[get_move_source(move)]
    //                 ,square_to_coordinates[get_move_target(move)]
    //                 ,promoted_pieces[get_move_prom(move)]
    //             );

    //GEMINI GENERATED
    int prom = get_move_prom(move);
    
    // Only print the 5th character if it is an actual promotion
    if (prom) {
        printf("%s%s%c\n", square_to_coordinates[get_move_source(move)],
                           square_to_coordinates[get_move_target(move)],
                           promoted_pieces[prom]);
    } else {
        // Standard 4-character move
        printf("%s%s\n", square_to_coordinates[get_move_source(move)],
                         square_to_coordinates[get_move_target(move)]);
    }

}

//print move list
void print_move_list(moves * move_list){

    //do nothing on empty move list
    if(move_list->count == 0) {
        printf("  no move in move list \n");
        return;
    }


    printf(" \n move   piece  capture  doublep  enpassant  castling\n");

    for(int move_count=0; move_count < move_list->count; move_count++){

        //init move
        int move = move_list->moves[move_count];

        //prrint move
        printf(" %s%s%c    %c       %d        %d         %d        %d\n",
                    square_to_coordinates[get_move_source(move)]
                    ,square_to_coordinates[get_move_target(move)]
                    ,get_move_prom(move) ? promoted_pieces[get_move_prom(move)] : ' ' //whitespace if no prom
                    ,ascii_pieces[get_move_piece(move)]
                    ,get_move_captureflag(move) ? 1:0
                    ,get_move_doublepflag(move) ? 1:0
                    ,get_move_enpassantflag(move) ? 1:0
                    ,get_move_castlingflag(move) ? 1:0

                );

        
    }

    //print total number of moves
    printf("\n\n   total number of moves: %d\n\n",move_list->count);

}



//preserve board state
#define copy_board()                                  \
    U64 bitboards_copy[12], occupancies_copy[3];      \
    int side_copy, enpassant_copy, castle_copy;       \
    memcpy(bitboards_copy, bitboards, 96);             \
    memcpy(occupancies_copy, occupancies, 24);         \
    side_copy = side, enpassant_copy = enpassant, castle_copy = castle; \
    U64 hash_key_copy = hash_key;  
          //sizeof(bitboards) = 96 bytes        
        //sizeof(occupancies) = 24 bytes 

//restore board state
#define take_back()                                    \
    memcpy(bitboards, bitboards_copy, 96);              \
    memcpy(occupancies, occupancies_copy, 24);              \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;  \
    hash_key = hash_key_copy;           


//move types
enum { all_moves, only_captures };


//if the rook gets captured we need a way of disabling castling that side
//in normal cases also handle here???
//(code snippet inside make_move function)
//our og function for castling only checks the king

//castling rights update constants
const int castling_rights[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};




//make move on chess board
static inline int make_move(int move, int move_flag){
    
    //quiet moves
    if(move_flag == all_moves){

        //preserve board state
        copy_board();

        //parse move
        int source_square = get_move_source(move);
        int target_square = get_move_target(move);
        int piece = get_move_piece(move);
        int promoted_piece = get_move_prom(move);

        int capture = get_move_captureflag(move);
        int doublep = get_move_doublepflag(move);
        int enpass = get_move_enpassantflag(move);
        int castling = get_move_castlingflag(move);

        //move piece
        pop_bit(bitboards[piece],source_square);
        set_bit(bitboards[piece], target_square);


        //copy_board, takeback func a little changed now
        //hash_key also included


        //hash piece 
        hash_key ^= piece_keys[piece][source_square]; //rem from source
        hash_key ^= piece_keys[piece][target_square]; //put on target sq

        //BAAKI HASH ADJUSTMENTS DONE NEECHE AS PER PROGRESS
        //I.E. ISNIDE CAPTURE IF BLOCK ETC


        //if piece prom
        //hash_key ^= piece_keys[piece][target_square];
        //hash_key ^= promoted_piece;

        //how to know old enpass square?
        //old castling?? etc
        //use prev baord state obtained from copy_board()

        //hash_key ^= enpassant_copy;
        //hash_key ^= enpass;

        //hash_key ^= castle_copy;
        //hash_key ^= castling;
        
        
        



        //side to move hashed later down in the function
        //when side^=1 is done just after this



        //handle capture moves
        if(capture){

            //pick up bitboard piece index ranges depending on side
            int start_piece, end_piece;

            //white to move
            if(side==white){
                start_piece = p;
                end_piece = k;
            }

            //black to move
            else{
                start_piece = P;
                end_piece = K;
            }

            //loop over bitboards opp to current side to move
            for(int bb_piece=start_piece;bb_piece<=end_piece;bb_piece++){

                //if there is a piece on the target square 
                if(get_bit(bitboards[bb_piece],target_square)){

                    //remove it from corresponding bitboard
                    pop_bit(bitboards[bb_piece],target_square);

                    //remove from hash key
                    hash_key ^= piece_keys[bb_piece][target_square];

                    break;
                }
            }

        }

        //handle pawn promotions
        if(promoted_piece){

            //erase pawn from target_square
            pop_bit(bitboards[ ((side==white) ? P:p) ], target_square);


            // --> HASH FIX: Remove the pawn from the target square in the hash key
            hash_key ^= piece_keys[piece][target_square];


            //set up promoted piece on chessboard
            set_bit(bitboards[promoted_piece], target_square);

            // --> HASH FIX: Add the new promoted piece to the target square in the hash key
            hash_key ^= piece_keys[promoted_piece][target_square];

        }

        //handle enpassant captures
        if(enpass){

            //erase pawn depending on side to move
            //(side==white) ?
            //    pop_bit(bitboards[p], target_square + 8 ) // + 8
            //    :  pop_bit(bitboards[P], target_square - 8 ); // - 8

            //white to move
            if(side==white){
                //remove captured pawn
                pop_bit(bitboards[p], target_square + 8 );

                //remove pawn from hashkey
                hash_key ^= piece_keys[p][target_square + 8];
            }
            //blac to move
            else{
                //remove captured pawn
                pop_bit(bitboards[P], target_square - 8 );

                //remove pawn from hashkey
                hash_key ^= piece_keys[P][target_square - 8];
            }


        }


         //hash enpassant (rem enpass sq from hashkey)
        if(enpassant != no_sq) hash_key^=enpassant_keys[enpassant];


        //reset enpassant sq to no_sq 
        //(unless double pawn move.  done below) 
        enpassant = no_sq; //global var is enpassant. reset this one 




        //handle double pawn push
        if(doublep){

            //set enpassant square depending on side to move
           //(side==white) ? (enpassant = target_square + 8) 
             //                   : (enpassant = target_square - 8);


            //white to move
            if(side==white){

                //set enpassatn sq
                enpassant = target_square + 8;

                //hash enpassant
                hash_key ^= enpassant_keys[target_square + 8];


            }
            //blac to move
            else{

                //set enpasant sq
                enpassant = target_square - 8;

                //hash enpassant
                hash_key ^= enpassant_keys[target_square - 8];

                
            }

        

        }

        //handle rook movement while castling
        if(castling){

            switch(target_square){
                //white kingside
                case(g1):
                    pop_bit(bitboards[R],h1);
                    set_bit(bitboards[R],f1);

                    //hash rook
                    hash_key ^= piece_keys[R][h1]; //rem rook from h1 from hash key
                    hash_key ^= piece_keys[R][f1]; //put rook on f1
                    //hash king??????????
                    //hash_key ^= piece_keys[K][e1]; //rem
                    //hash_key ^= piece_keys[K][g1]; //put

                    break;

                //white queen side
                case(c1):
                    pop_bit(bitboards[R],a1);
                    set_bit(bitboards[R],d1);

                    //hash rook
                    hash_key ^= piece_keys[R][a1]; //rem rook
                    hash_key ^= piece_keys[R][d1]; //put rook

                    break;

                //blaack kingside
                case(g8):
                    pop_bit(bitboards[r],h8);
                    set_bit(bitboards[r],f8);

                    //hash rook
                    hash_key ^= piece_keys[r][h8]; //rem rook
                    hash_key ^= piece_keys[r][f8]; //put rook 

                    break;

                //black queen side
                case(c8):
                    pop_bit(bitboards[r],a8);
                    set_bit(bitboards[r],d8);

                    //hash rook
                    hash_key ^= piece_keys[r][a8]; //rem rook
                    hash_key ^= piece_keys[r][d8]; //put rook 

                    break;

            }

        }


        //hash castling
        //exclude all rights, then put them 
        //(put them after updation of castling rights a few lines below)
        hash_key ^= castle_keys[castle];


        //update castling rights (castling moves handled above)
        castle &= castling_rights[source_square];
        castle &= castling_rights[target_square]; 

        //hash castling (put the rights back in)
        hash_key ^= castle_keys[castle];


        

        //reset occupancies
        memset(occupancies,0ULL,24);

        //loop over white pieces bitboards
        for(int bb_piece=P;bb_piece<=K;bb_piece++){
            //update white occupancies
            occupancies[white] |= bitboards[bb_piece];
        }
        //loop over black pieces bitboards
        for(int bb_piece=p;bb_piece<=k;bb_piece++){
            //update black occupancies
            occupancies[black] |= bitboards[bb_piece];
        }
        //upodate both sides occs
        occupancies[both] |= occupancies[white] | occupancies[black];



        //change side
        side ^= 1;


        //hash side
        hash_key ^= side_key;


        // // DEBUG HASH KEY INCREMENTAL UPDATE

        // //build hash key for the updated pos 
        // //(after move is made) from scratch
        // U64 hash_from_scratch = generate_hash_key();

        // //in case if hash key built from scratch doesnt match
        // //with the hashkey that was built from incremental update
        // //then interrupt execution
        // if(hash_key !=  hash_from_scratch){

        //     printf("Make move\n");
        //     printf("move: "); print_move(move);
        //     print_board();
        //     printf("hash key should be: %llx \n",hash_from_scratch);
        //     getchar();


        // }
        




        //make sure that king has not been exposed into a chceck
        if( is_square_attacked( ((side==white) ? //newly switched side
            get_ls1b_index(bitboards[k]) :  get_ls1b_index(bitboards[K]))
             , side) )   {

            //move is illegal. take it back
            take_back(); //(board state was copied at the start of the function)

            //return illegal move
            return 0;
        }
        else{
            return 1; //return legal move
        }




    }

    //capture moves
    else{
        //make sure move is capture
        if(get_move_captureflag(move)){
            return make_move(move, all_moves);
        }

        //otherwise not a capture
        else return 0; //dont make the move

    }


}


//generate all moves
static inline void generate_moves(moves * move_list){

    //init move count
    move_list->count = 0;

    //declare source and target squares
    int source_square;
    int target_square;

    //declare current piece's bitboard copy and its attacks
    U64 bitboard, attacks;
    
    //loop over all the bitboards
    for(int piece=P;piece<=k;piece++){

        //init piece bitboard copy
        bitboard = bitboards[piece];

        //generate white pawns and white king castling moves
        if(side==white){

            //pickup white pawn bitboards index
            if(piece==P){

                //loop over white pawns within white pawn bitboard
                while(bitboard){

                    //init source square
                    source_square = get_ls1b_index(bitboard);
                    //printf("white pawn: %s\n",square_to_coordinates[source_square]);

                    //init target square
                    target_square = source_square - 8; //white hence  minus 8

                    //generate quite pawn moves
                    if( !(target_square<a8) && !get_bit(occupancies[both],target_square)){
                            //if targetsq is within range and is empty (for quiet pawn move)

                            //pawn promotion
                            if((source_square>=a7) && (source_square<=h7)){

                                //add move into a move list

                                // printf("pawn prom: %s%sq\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sr\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sb\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sn\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);

                                add_move(move_list, encode_move(source_square,target_square,piece,Q,0,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,R,0,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,B,0,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,N,0,0,0,0));

                            }
                            else{
                                //one square ahead pawn move

                                //printf("pawn push: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));


                                //double squares aahead pawn move
                                if((source_square>=a2) && (source_square<=h2) 
                                    && !get_bit(occupancies[both],target_square-8)){

                                    //printf("double pawn push: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square-8]);
                                                //target_square-8
                                    add_move(move_list, encode_move(source_square, target_square-8, piece,0,0,1,0,0));
                                                                                //targetsq - 8

                                }
                            }

                    }       


                    //init pawn attacks bitboard
                    attacks = pawn_attacks[side][source_square] & occupancies[black];

                    //generate pawn captures
                    while(attacks){

                        //init target square
                        target_square = get_ls1b_index(attacks);

                        //pawn promotion
                        if((source_square>=a7) && (source_square<=h7)){

                            //add move into a move list

                            // printf("pawn prom capture: %s%sq\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn prom capture: %s%sr\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn prom capture: %s%sb\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn promcapture : %s%sn\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);

                            add_move(move_list, encode_move(source_square,target_square,piece,Q,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,R,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,B,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,N,1,0,0,0));


                        }
                        else{
                            //one square ahead pawn move

                            //printf("pawn capture: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                        }

                        //pop ls1b of pawn attacks
                        pop_bit(attacks,target_square);

                    }

                    //generate enpassant captures
                    if(enpassant != no_sq){

                        //lookup pawn attacks and bitwise AND with enpassant square bit
                        U64 enpassant_attacks = 
                            pawn_attacks[side][source_square] & (1ULL<<enpassant);

                        //make sure enpassant capture is available
                        if(enpassant_attacks){

                            //init enpassant capture target square
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            //printf("pawn enpassant capture: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_enpassant]);
                                                          //[target_enpassant]
                            add_move(move_list, encode_move(source_square,target_enpassant,piece,0,1,0,1,0));

                        }

                    }

                    //pop ls1b from piece bitboard copy
                    pop_bit(bitboard, source_square);
                }
            }

            //castling moves
            if(piece==K){

                //king side castle is available
                if(castle & wk){

                    //make sure squares between king and king's rook are empty
                    if( !get_bit(occupancies[both],f1) && !get_bit(occupancies[both],g1) ){
                        //get_bit==0

                        //make sure king and the f1 square are not under attacks
                        if( !is_square_attacked(e1,black) && 
                                !is_square_attacked(f1,black) ){
                        //WHY NOT CONSIDER g1 HERE?????????
                            //printf("castle kingside: e1g1\n");
                            add_move(move_list, encode_move(e1,g1,piece,0,0,0,0,1));


                        }

                    }
                }
                //queen side castling is available
                if(castle & wq){

                    //make sure squares between king and queen's rook are empty
                    if( !get_bit(occupancies[both],d1) && !get_bit(occupancies[both],c1) 
                            && !get_bit(occupancies[both],b1)){
                        //get_bit==0

                        //make sure king and the d1 square are not under attacks
                        if( !is_square_attacked(e1,black) && 
                                !is_square_attacked(d1,black) ){
                        //WHY NOT CONSIDER c1 HERE?????????
                            //printf("castle queenside: e1c1\n");
                            add_move(move_list, encode_move(e1,c1,piece,0,0,0,0,1));


                        }

                    }
                }


            }




        }
        //generate black pawns and black king castling moves
        else{

            //pickup black pawn bitboards index
            if(piece==p){

                //loop over white pawns within white pawn bitboard
                while(bitboard){

                    //init source square
                    source_square = get_ls1b_index(bitboard);
                    //printf("black pawn: %s\n",square_to_coordinates[source_square]);

                    //init target square
                    target_square = source_square + 8; //black hence  plus 8

                    //generate quite pawn moves
                    if( !(target_square>h1) && !get_bit(occupancies[both],target_square)){
                            //if targetsq is within range and is empty (for quiet pawn move)

                            //pawn promotion
                            if((source_square>=a2) && (source_square<=h2)){

                                //add move into a move list

                                // printf("pawn prom: %s%sq\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sr\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sb\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                // printf("pawn prom: %s%sn\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);

                                add_move(move_list, encode_move(source_square,target_square,piece,q,1,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,r,1,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,b,1,0,0,0));
                                add_move(move_list, encode_move(source_square,target_square,piece,n,1,0,0,0));


                            }
                            else{
                                //one square ahead pawn move
                                //printf("pawn push: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                                add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));


                                //double squares aahead pawn move
                                if((source_square>=a7) && (source_square<=h7) 
                                    && !get_bit(occupancies[both],target_square+8)){

                                    //printf("double pawn push: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square+8]);
                                                                //target_square+8
                                    add_move(move_list, encode_move(source_square,target_square+8,piece,0,0,1,0,0));

                                }
                            }

                    }       

                    //init pawn attacks bitboard
                    attacks = pawn_attacks[side][source_square] & occupancies[white];

                    //generate pawn captures
                    while(attacks){

                        //init target square
                        target_square = get_ls1b_index(attacks);

                        //pawn promotion
                        if((source_square>=a2) && (source_square<=h2)){

                            //add move into a move list

                            // printf("pawn prom capture: %s%sq\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn prom capture: %s%sr\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn prom capture: %s%sb\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            // printf("pawn promcapture : %s%sn\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);

                            add_move(move_list, encode_move(source_square,target_square,piece,q,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,r,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,b,1,0,0,0));
                            add_move(move_list, encode_move(source_square,target_square,piece,n,1,0,0,0));


                        }
                        else{
                            //one square ahead pawn move
                            //printf("pawn capture: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                            add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                        }

                        //pop ls1b of pawn attacks
                        pop_bit(attacks,target_square);

                    }

                    //generate enpassant captures
                    if(enpassant != no_sq){

                        //lookup pawn attacks and bitwise AND with enpassant square bit
                        U64 enpassant_attacks = 
                            pawn_attacks[side][source_square] & (1ULL<<enpassant);

                        //make sure enpassant capture is available
                        if(enpassant_attacks){

                            //init enpassant capture target square
                            int target_enpassant = get_ls1b_index(enpassant_attacks);
                            //printf("pawn enpassant capture: %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_enpassant]);
                                                    //[target_enpassant]
                            add_move(move_list, encode_move(source_square,target_enpassant,piece,0,1,0,1,0));

                        }

                    }

                    //pop ls1b from piece bitboard copy
                    pop_bit(bitboard, source_square);
                }
            }

             //castling moves
            if(piece==k){

                //king side castle is available
                if(castle & bk){

                    //make sure squares between king and king's rook are empty
                    if( !get_bit(occupancies[both],f8) && !get_bit(occupancies[both],g8) ){
                        //get_bit==0

                        //make sure king and the f8 square are not under attacks
                        if( !is_square_attacked(e8,white) && 
                                !is_square_attacked(f8,white) ){
                        //WHY NOT CONSIDER g8 HERE?????????
                            //printf("castle kingside: e8g8\n");
                            add_move(move_list, encode_move(e8,g8,piece,0,0,0,0,1));


                        }

                    }
                }
                //queen side castling is available
                if(castle & bq){

                    //make sure squares between king and queen's's rook are empty
                    if( !get_bit(occupancies[both],d8) && !get_bit(occupancies[both],c8) 
                            && !get_bit(occupancies[both],b8)){
                        //get_bit==0

                        //make sure king and the d8 square are not under attacks
                        if( !is_square_attacked(e8,white) && 
                                !is_square_attacked(d8,white) ){
                        //WHY NOT CONSIDER c8 HERE?????????
                            //printf("castle queenside: e8c8\n");
                            add_move(move_list, encode_move(e8,c8,piece,0,0,0,0,1));


                        }

                    }
                }


            }


        } 

        //generate knight moves
        if( (side==white) ? piece==N : piece==n ){

            //loop over source squares of piece bitboard copy
            while(bitboard){

                //init source square
                source_square = get_ls1b_index(bitboard);

                //init piece attacks in order to get set of target squares
                attacks = knight_attacks[source_square]
                     & (~occupancies[side]);
                // ! is logical not
                // ~ is bitwise not
                // &= occupancies[!side] is invalid 
                //becausde it only considers opp colour but not empty squares
                //??????????????

                //loop over target squares available from generated attacks
                while(attacks){

                    //init tagrte square
                    target_square = get_ls1b_index(attacks);

                    //quite move    
                    if( !get_bit( ((side==white) ? occupancies[black] : occupancies[white]) , target_square )){

                        //printf("piece quiet : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));

                    }
                    //capture move
                    else{
                        //printf("piece capture : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                    }


                    //pop ls1b in current attacks set
                    pop_bit(attacks,target_square);

                }


                //pop ls1b of the current piece bitboard array
                pop_bit(bitboard,source_square);

            }

        }


        //generate bishop moves
        if( (side==white) ? piece==B : piece==b ){

            //loop over source squares of piece bitboard copy
            while(bitboard){

                //init source square
                source_square = get_ls1b_index(bitboard);

                //init piece attacks in order to get set of target squares
                attacks = get_bishop_attacks(source_square,occupancies[both])
                     & (~occupancies[side]);
                // ! is logical not
                // ~ is bitwise not
                // &= occupancies[!side] is invalid 
                //becausde it only considers opp colour but not empty squares
                //??????????????

                //loop over target squares available from generated attacks
                while(attacks){

                    //init tagrte square
                    target_square = get_ls1b_index(attacks);

                    //quite move    
                    if( !get_bit( ((side==white) ? occupancies[black] : occupancies[white]) , target_square )){

                        //printf("piece quiet : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));

                    }
                    //capture move
                    else{
                        //printf("piece capture : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                    }


                    //pop ls1b in current attacks set
                    pop_bit(attacks,target_square);

                }


                //pop ls1b of the current piece bitboard array
                pop_bit(bitboard,source_square);

            }

        }


        //generate rook moves
        if( (side==white) ? piece==R : piece==r ){

            //loop over source squares of piece bitboard copy
            while(bitboard){

                //init source square
                source_square = get_ls1b_index(bitboard);

                //init piece attacks in order to get set of target squares
                attacks = get_rook_attacks(source_square,occupancies[both])
                     & (~occupancies[side]);
                // ! is logical not
                // ~ is bitwise not
                // &= occupancies[!side] is invalid 
                //becausde it only considers opp colour but not empty squares
                //??????????????

                //loop over target squares available from generated attacks
                while(attacks){

                    //init tagrte square
                    target_square = get_ls1b_index(attacks);

                    //quite move    
                    if( !get_bit( ((side==white) ? occupancies[black] : occupancies[white]) , target_square )){

                        //printf("piece quiet : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));

                    }
                    //capture move
                    else{
                        //printf("piece capture : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                    }


                    //pop ls1b in current attacks set
                    pop_bit(attacks,target_square);

                }


                //pop ls1b of the current piece bitboard array
                pop_bit(bitboard,source_square);

            }

        }


        //generate queen moves
        if( (side==white) ? piece==Q : piece==q ){

            //loop over source squares of piece bitboard copy
            while(bitboard){

                //init source square
                source_square = get_ls1b_index(bitboard);

                //init piece attacks in order to get set of target squares
                attacks = get_queen_attacks(source_square,occupancies[both])
                     & (~occupancies[side]);
                // ! is logical not
                // ~ is bitwise not
                // &= occupancies[!side] is invalid 
                //becausde it only considers opp colour but not empty squares
                //??????????????

                //loop over target squares available from generated attacks
                while(attacks){

                    //init tagrte square
                    target_square = get_ls1b_index(attacks);

                    //quite move    
                    if( !get_bit( ((side==white) ? occupancies[black] : occupancies[white]) , target_square )){

                        //printf("piece quiet : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));

                    }
                    //capture move
                    else{
                        //printf("piece capture : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                    }


                    //pop ls1b in current attacks set
                    pop_bit(attacks,target_square);

                }


                //pop ls1b of the current piece bitboard array
                pop_bit(bitboard,source_square);

            }

        }


        //generate king moves
        if( (side==white) ? piece==K : piece==k ){

            //loop over source squares of piece bitboard copy
            while(bitboard){

                //init source square
                source_square = get_ls1b_index(bitboard);

                //init piece attacks in order to get set of target squares
                attacks = king_attacks[source_square]
                     & (~occupancies[side]);
                // ! is logical not
                // ~ is bitwise not
                // &= occupancies[!side] is invalid 
                //becausde it only considers opp colour but not empty squares
                //??????????????

                //loop over target squares available from generated attacks
                while(attacks){

                    //init tagrte square
                    target_square = get_ls1b_index(attacks);

                    //quite move    
                    if( !get_bit( ((side==white) ? occupancies[black] : occupancies[white]) , target_square )){

                        //printf("piece quiet : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,0,0,0,0));

                    }
                    //capture move
                    else{
                        //printf("piece capture : %s%s\n", square_to_coordinates[source_square], square_to_coordinates[target_square]);
                        add_move(move_list, encode_move(source_square,target_square,piece,0,1,0,0,0));

                    }


                    //pop ls1b in current attacks set
                    pop_bit(attacks,target_square);

                }

                //pop ls1b of the current piece bitboard array
                pop_bit(bitboard,source_square);

            }

        }


    }



}



//PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT
//PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT
//PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT
//PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT PERFT


//get time in milliseconds
long long get_time_ms(){

    #ifdef _WIN32
        return GetTickCount();
    #else
        struct timeval time_value;
        gettimeofday( &time_value , NULL);
        return time_value.tv_sec*1000 + timevalue.tv_usec/1000 ;

    #endif

}

//leaf nodes (number of positions reached
    // during the test of move generator at a given depth)
unsigned long long nodes; //init 0 by default

//perft driver
static inline void perft_driver(int depth){

    //recursion escape condition
    if(depth==0){
        //increment nodes count
        nodes++;
        return;
    }

    //create move list
    moves move_list[1];

    //generate moves
    generate_moves(move_list);

    //loop over generated moves
    for(int move_count=0; move_count < move_list->count; move_count++){

        //init move (now defined inside make_move 's argument
        //int move = move_list->moves[move_count];

        //preserve board state
        copy_board();

        //make move
        if( !make_move( move_list->moves[move_count], all_moves) ){
            //skip to next iter
            continue;
        }
    
        //call perft driver recursively
        perft_driver(depth-1);

        //takeback
        take_back();

        //DONT USE THIS ONE????????
        //USE THE MAKE_MOVE HASH DEBUG ????
        
        // // DEBUG HASH KEY INCREMENTAL UPDATE

        // //build hash key for the updated pos 
        // //(after move is made) from scratch
        // U64 hash_from_scratch = generate_hash_key();

        // //in case if hash key built from scratch doesnt match
        // //with the hashkey that was built from incremental update
        // //then interrupt execution
        // if(hash_key !=  hash_from_scratch){

        //     printf("Take back\n");
        //     printf("move: "); print_move(move_list->moves[move_count]);
        //     print_board();
        //     printf("hash key should be: %llx \n",hash_from_scratch);
        //     //getchar(); //fakking gui murderer


        // }

    }

}

//perft test
void perft_test(int depth){

    printf("\n    Performance Test\n\n");

     //create move list
    moves move_list[1];

    //generate moves
    generate_moves(move_list);

    //start tracking time
    long long start = get_time_ms();

    //loop over generated moves
    for(int move_count=0; move_count < move_list->count; move_count++){

        //init move (now defined inside make_move 's argument
        //int move = move_list->moves[move_count];

        //preserve board state
        copy_board();

        //make move
        if( !make_move( move_list->moves[move_count], all_moves) ){
            //skip to next iter
            continue;
        }

        // cummulative nodes
        long cummulative_nodes = nodes;
    
        //call perft driver recursively
        perft_driver(depth-1);

         // old nodes
        long old_nodes = nodes - cummulative_nodes;

        //takeback
        take_back();

        //print move
        printf("     move: %s%s%c  nodes: %ld\n", square_to_coordinates[get_move_source(move_list->moves[move_count])],
                                                  square_to_coordinates[get_move_target(move_list->moves[move_count])],
                                                  get_move_prom(move_list->moves[move_count]) ? promoted_pieces[get_move_prom(move_list->moves[move_count])] : ' ',
                                                  old_nodes);
    }
    
     //end tracking time then calculate time taken
    long end = get_time_ms();
    //printf("time taken to execute (ms): %d\n",end-start);

    // print results
    printf("\n    Depth: %d\n", depth);
    printf("    Nodes: %lld\n", nodes);
    printf("     Time: %ld\n\n", end - start);

}
















// Assuming these are defined globally or in your NNUE namespace
// extern float FeatureWeights[768][512];
// constexpr int L1_SIZE = 512;


// Call this ONCE when a new FEN is parsed or a new game starts
// void RefreshAccumulator(Accumulator& acc) {
//     // 1. Start with the biases
//     for (int i = 0; i < L1_SIZE; ++i) {
//         acc.values[i] = FeatureBiases[i];
//     }
    
//     // 2. Add weights for all pieces currently on the bitboards
//     for (int piece = P; piece <= k; ++piece) {
//         U64 bb = bitboards[piece];
//         while (bb) {
//             int sq = get_ls1b_index(bb);
//             int feature = GetFeatureIndex(piece, sq);
            
//             for (int i = 0; i < L1_SIZE; ++i) {
//                 acc.values[i] += FeatureWeights[feature][i];
//             }
//             pop_bit(bb, sq);
//         }
//     }
// }

// clipped_relu is applied ONLY inside EvaluateNNUE, not here
void RefreshAccumulator(Accumulator& acc/*, U64 bitboards[12]*/) {
    
    // reset to biases
    for (int i = 0; i < L1_SIZE; i++)
        acc.values[i] = FeatureBiases[i];

    // piece features (indices 0–767)
    for (int piece_type = 0; piece_type < 12; piece_type++) {
        U64 bb = bitboards[piece_type];
        while (bb) {
            int square = __builtin_ctzll(bb);
            bb &= bb - 1;
            int feature_index = piece_type * 64 + square;
            for (int i = 0; i < L1_SIZE; i++)
                acc.values[i] += FeatureWeights[feature_index][i];
        }
    }


    //  // index 768: side to move
    // if (side == white) {
    //     for (int i = 0; i < L1_SIZE; i++)
    //         acc.values[i] += FeatureWeights[768][i];
    // }

    // // indices 769-772: castling rights
    // if (castle & wk) for (int i = 0; i < L1_SIZE; i++) acc.values[i] += FeatureWeights[769][i];
    // if (castle & wq) for (int i = 0; i < L1_SIZE; i++) acc.values[i] += FeatureWeights[770][i];
    // if (castle & bk) for (int i = 0; i < L1_SIZE; i++) acc.values[i] += FeatureWeights[771][i];
    // if (castle & bq) for (int i = 0; i < L1_SIZE; i++) acc.values[i] += FeatureWeights[772][i];

    // // indices 773-780: en passant file
    // if (enpassant != no_sq) {
    //     int ep_file = enpassant % 8;  // file 0-7
    //     for (int i = 0; i < L1_SIZE; i++)
    //         acc.values[i] += FeatureWeights[773 + ep_file][i];
    // }



}

// Call this incrementally during search
void UpdateAccumulatorForMove(Accumulator& next_acc, const Accumulator& prev_acc, int move) {
    // Copy parent state
    next_acc.copy_from(prev_acc);
    
    // Decode the move
    int source = get_move_source(move);
    int target = get_move_target(move);
    int piece = get_move_piece(move);
    int prom = get_move_prom(move);
    int capture = get_move_captureflag(move);
    int enpass = get_move_enpassantflag(move);
    int castling = get_move_castlingflag(move);
    
    int added[3], removed[3];
    int add_count = 0, rem_count = 0;

    // 1. Move the piece
    removed[rem_count++] = GetFeatureIndex(piece, source);
    if (prom) {
        added[add_count++] = GetFeatureIndex(prom, target);
    } else {
        added[add_count++] = GetFeatureIndex(piece, target);
    }

    // 2. Handle standard captures
    if (capture && !enpass) {
        int captured_piece = -1;
        int start_piece = (side == white) ? p : P;
        int end_piece = (side == white) ? k : K;
        
        // Find what piece was sitting on the target square
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
            if (get_bit(bitboards[bb_piece], target)) {
                captured_piece = bb_piece;
                break;
            }
        }
        if (captured_piece != -1) {
            removed[rem_count++] = GetFeatureIndex(captured_piece, target);
        }
    }

    // 3. Handle En Passant captures
    if (enpass) {
        int cap_sq = (side == white) ? target + 8 : target - 8;
        int captured_pawn = (side == white) ? p : P;
        removed[rem_count++] = GetFeatureIndex(captured_pawn, cap_sq);
    }

    // 4. Handle Castling (King is already moved by step 1, just move the rook)
    if (castling) {
        int rook_piece = (side == white) ? R : r;
        int rook_source, rook_target;
        
        if (target == g1)      { rook_source = h1; rook_target = f1; } // WK
        else if (target == c1) { rook_source = a1; rook_target = d1; } // WQ
        else if (target == g8) { rook_source = h8; rook_target = f8; } // BK
        else if (target == c8) { rook_source = a8; rook_target = d8; } // BQ
        
        removed[rem_count++] = GetFeatureIndex(rook_piece, rook_source);
        added[add_count++] = GetFeatureIndex(rook_piece, rook_target);
    }

    // 5. Apply the net changes to the neurons (loop optimized for SIMD)
    for (int i = 0; i < L1_SIZE; ++i) {
        float delta = 0.0f;
        for (int j = 0; j < rem_count; ++j) delta -= FeatureWeights[removed[j]][i];
        for (int j = 0; j < add_count; ++j) delta += FeatureWeights[added[j]][i];
        next_acc.values[i] += delta;
    }
}








//EVALUATE EVALUATE EVALUATE EVALUATE
//EVALUATE EVALUATE EVALUATE EVALUATE



// not implementing   material_score[12] array
//for now, evaluate function is left empty
//to be later used when NN

static inline int evaluate(){
    
    // 1. Evaluate the position using the accumulator for the current ply
    int score = static_cast<int>(EvaluateNNUE(acc_stack[ply]));

    //printf("NNUE eval: %d cp (side=%d)\n", score, side);           // temp debug

    // 2. Adjust perspective for Negamax
    // Since your neural network was trained on absolute FENs 
    // (positive = White is winning), we must invert it if it is Black's turn.
    return (side == white) ? score : -score;

}





// SEARCG SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH
// SEARCG SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH
// SEARCG SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH
// SEARCG SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH SEARCH


// most valuable victim & less valuable attacker

/*
                          
    (Victims) Pawn Knight Bishop   Rook  Queen   King
  (Attackers)
        Pawn   105    205    305    405    505    605
      Knight   104    204    304    404    504    604
      Bishop   103    203    303    403    503    603
        Rook   102    202    302    402    502    602
       Queen   101    201    301    401    501    601
        King   100    200    300    400    500    600

*/

// MVV LVA [attacker][victim]
static int mvv_lva[12][12] = {
 	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};





//THIS PORTION MOVED TO TOP HENCE COMMENTED HERE
//(FROM INT PLY TO INT BEST MOVE)

// //half move counter
// int ply;

// // --- Add this array for NNUE ---       //NNUE ACCUMULATOR ARRAY
// #define MAX_PLY 128
// Accumulator acc_stack[MAX_PLY];

// // Helper to get the 0-767 index for the neural net
// inline int GetFeatureIndex(int piece, int square) {
//     return piece * 64 + square;
// }

// //best move
// int best_move;




// --- Time Management Globals ---
int time_set = 0;      // 1 if a time limit is active, 0 otherwise
int stopped = 0;       // 1 if the search needs to abort immediately
long long stop_time = 0;     // The exact millisecond the engine must stop thinking







// --- Transposition Table Read/Write ---                                   //BY GEMINI
static inline int read_hash_entry(int alpha, int beta, int depth, int *tt_best_move) {
    // Fast modulo using bitwise AND (works because hash_entries is a power of 2)
    tt_entry *entry = &hash_table[hash_key & (hash_entries - 1)];

    if (entry->hash_key == hash_key) {
        // Always extract the best move to help with move ordering!
        *tt_best_move = entry->best_move;

        // If the position was searched to a depth equal or greater than what we need now
        if (entry->depth >= depth) {
            int score = entry->score;

            // Retrieve mate scores relative to the current ply
            if (score < -48000) score += ply;
            if (score > 48000) score -= ply;

            if (entry->flag == hash_exact) return score;
            if ((entry->flag == hash_alpha) && (score <= alpha)) return alpha; // Fail-low
            if ((entry->flag == hash_beta) && (score >= beta)) return beta;    // Fail-high
        }
    }
    // Dummy value to indicate no useful TT hit
    return 100000; 
}

static inline void write_hash_entry(int score, int depth, int hash_flag, int best_move) {
    tt_entry *entry = &hash_table[hash_key & (hash_entries - 1)];

    // Store mate scores as an absolute distance from the root
    if (score < -48000) score -= ply;
    if (score > 48000) score += ply;

    // Always Replace scheme (simplest and very effective)
    entry->hash_key = hash_key;
    entry->score = score;
    entry->flag = hash_flag;
    entry->depth = depth;
    entry->best_move = best_move;
}









// score moves
static inline int score_move(int move, int tt_best_move)     //tt_best_move  wrt TRANSPOSITION TABLE
{

    // If this move is the best move found in the Transposition Table, search it FIRST
    if (move == tt_best_move) return 30000;

    

    // score capture move
    if (get_move_captureflag(move))
    {
        // init target piece
        int target_piece = P;
        
        // pick up bitboard piece index ranges depending on side
        int start_piece, end_piece;
        
        // pick up side to move
        if (side == white) { start_piece = p; end_piece = k; }
        else { start_piece = P; end_piece = K; }
        
        // loop over bitboards opposite to the current side to move
        for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
        {
            // if there's a piece on the target square
            if (get_bit(bitboards[bb_piece], get_move_target(move)))
            {
                // remove it from corresponding bitboard
                target_piece = bb_piece;
                break;
            }
        }
                
        // score move by MVV LVA lookup [source piece][target piece]
        return mvv_lva[get_move_piece(move)][target_piece];
    }
    
    // score quiet move
    else
    {
    
    }
    
    return 0;
}



// sort moves in descending order
static inline void sort_moves(moves *move_list, int tt_best_move)     //tt_best_move wrt TRANSPOSITION TABLE
{
    // move scores
    int move_scores[move_list->count]; //new array
    //printf("\n\n");   caused the fucking uci to jam.
    // score all the moves within a move list
    for (int count = 0; count < move_list->count; count++)
        // score move
        move_scores[count] = score_move(move_list->moves[count],tt_best_move);  //pass in this func call
    
    // loop over current move within a move list
    for (int current_move = 0; current_move < move_list->count; current_move++)
    {
        // loop over next move within a move list
        for (int next_move = current_move + 1; next_move < move_list->count; next_move++)
        {
            // compare current and next move scores
            if (move_scores[current_move] < move_scores[next_move])
            {
                // swap scores
                int temp_score = move_scores[current_move];
                move_scores[current_move] = move_scores[next_move];
                move_scores[next_move] = temp_score;
                
                // swap moves
                int temp_move = move_list->moves[current_move];
                move_list->moves[current_move] = move_list->moves[next_move];
                move_list->moves[next_move] = temp_move;
            }
        }
    }
}




// print move scores
void print_move_scores(moves *move_list)
{
    printf("     Move scores:\n\n");
        
    // loop over moves within a move list
    for (int count = 0; count < move_list->count; count++)
    {
        printf("     move: ");
        print_move(move_list->moves[count]);
        printf(" score: %d\n", score_move(move_list->moves[count], 0) );    //pass 0 here
    }
}






// Cross-platform non-blocking input listener
int input_waiting() {
#ifndef _WIN32
    fd_set readfds;
    struct timeval tv;
    FD_ZERO (&readfds);
    FD_SET (fileno(stdin), &readfds);
    tv.tv_sec=0; tv.tv_usec=0;
    select(16, &readfds, 0, 0, &tv);
    return (FD_ISSET(fileno(stdin), &readfds));
#else
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;
    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }
    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
        return dw;
    } else {
        GetNumberOfConsoleInputEvents(inh, &dw);
        return dw <= 1 ? 0 : dw;
    }
#endif
}

// Read standard input from the GUI mid-search
void read_input() {
    int bytes;
    char input[256] = "", *endc;

    if (input_waiting()) {
       
        do {
            bytes = read(fileno(stdin), input, 256);
        } while (bytes < 0);
        
        endc = strchr(input, '\n');
        if (endc) *endc = 0;

        if (strlen(input) > 0) {
            if (!strncmp(input, "quit", 4)) {
                exit(0);
            }
            else if (!strncmp(input, "stop", 4)) {
                stopped = 1;
                //only stop when GUI explicitly tells to stop
            }
        }
    }
}

// Check time and GUI input
static inline void check_up() {
    // Check if time is up
    if (time_set == 1 && get_time_ms() >= stop_time) {
        stopped = 1;
    }

    // Check if the GUI sent a command
        // DO NOT call read_input() here. 
        // It causes a fatal Windows pipe deadlock.
        //read_input();
}











static inline int quiescence(int alpha, int beta){


// Prevent array out-of-bounds crashes
    if (ply >= MAX_PLY - 1) return evaluate();


    // --- TIME CHECKING IN QUIESCENCE ---
    if ((nodes & 2047) == 0) {
        check_up();
    }
    // Abort if time ran out
    if (stopped == 1) return 0;




    //increment nodes count
    nodes++; 

    //evaluate pos
    int evaluation = evaluate();

    //fail-hard beta cutoff
    if(evaluation>=beta) return beta; //node (move) fails high

    //found a better move
    if(evaluation > alpha){
        //PV node (move)
        alpha = evaluation;
    }


    //create move list instance
    moves move_list[1];

    //generate moves();
    generate_moves(move_list);


    //sort moves     //move ordering
    sort_moves(move_list,0);               // Pass 0 because we don't use the TT in Q-search)
    //in negamax: 1.7 mil down to 99k nodes because of move ordering
    //in quiescence search: 99k down to 1k nodes

    //loop over moves within a move list
    for(int count=0;count<move_list->count;count++){

        //preserve board state
        copy_board();


// --- NNUE ACCUMULATOR UPDATE ---
        UpdateAccumulatorForMove(acc_stack[ply + 1], acc_stack[ply], move_list->moves[count]);


        
        //increment ply
        ply++;

        //make sure to make only legal moves
        if( make_move( move_list->moves[count] , only_captures ) == 0){

            //0 means illegal move

            //decrement ply
            ply--;

            //skip to next move
            continue;
        }

        //score currennt move
        //alpha=-beta, beta=-alpha for the other side now
        int score = -quiescence(-beta, -alpha); 
        
        //decrement ply
        ply--;

        //take move back
        take_back();

        //fail-hard beta cutoff
        if(score>=beta) return beta; //node (move) fails high

        //found a better move
        if(score > alpha){
            //PV node (move)
            alpha = score;
        }
    }

    //node (move) fails low
    return alpha;

}


//negamax alpha beta search
static inline int negamax( int alpha, int beta, int depth){

// Prevent array out-of-bounds crashes
    if (ply >= MAX_PLY - 1) return evaluate();


// --- TIME & INPUT CHECKING ---
    // Poll the time and GUI every 2048 nodes to avoid massive performance drops
    if ((nodes & 2047) == 0) {
        check_up();
    }
    
    // If time is up or the GUI sent "stop", immediately abort this branch.
    // Return 0 because the calculation is incomplete and untrustworthy.
    if (stopped == 1) return 0;
    
    // recursion base case + quiescence search
    if (depth == 0) {
        return quiescence(alpha, beta);
        //return evaluate(); //now called inside quiescence
    }

    //inc nodes count
    nodes++;


    // --- MATE DISTANCE PRUNING ---     //by GEMINI
    // If we find a shorter mate, we don't care about longer ones.
    // We constrain the alpha/beta window to the max possible mate score at this ply.
    int mate_value = 49000 - ply;
    if (alpha < -mate_value) alpha = -mate_value;
    if (beta > mate_value - 1) beta = mate_value - 1;
    if (alpha >= beta) return alpha;
    // -----------------------------





    //is king in check
    int in_check = is_square_attacked( 
        (side==white) ? get_ls1b_index(bitboards[K]) :  get_ls1b_index(bitboards[k]) , side^1  
            );

    
    //increase search depth if the king has been exposed to a check
    if(in_check) depth++;



    // --- 1. READ FROM TT ---               //TRANSPOSITION TABLE PORTION BY GEMINI
    int hash_flag = hash_alpha; // Assume we fail low until proven otherwise
    int tt_best_move = 0;
    
    // Read the entry. If we get a hit, return immediately (unless we are at the root)
    int tt_score = read_hash_entry(alpha, beta, depth, &tt_best_move);
    if (ply > 0 && tt_score != 100000) {
        return tt_score; 
    }





    //legal moves counter
    int legal_moves = 0;
    
    //best move so far  (in this node)
    int best_sofar;

    //old value of alpha
    int old_alpha = alpha;

    //create move list instance
    moves move_list[1];

    //generate moves();
    generate_moves(move_list);

    
    //sort moves     //move ordering
    // --- 2. SORT WITH TT MOVE ---
    sort_moves(move_list, tt_best_move);    //NOW ALSO INCLUDES tt_best_move wrt TRANS. TABLE
    //in negamax: 1.7 mil down to 99k nodes because of move ordering
    //in quiescence search: 99k down to 1k nodes
                            

    //loop over moves within a move list
    for(int count=0;count<move_list->count;count++){

        //preserve board state
        copy_board();
        

// --- NNUE ACCUMULATOR UPDATE ---
        UpdateAccumulatorForMove(acc_stack[ply + 1], acc_stack[ply], move_list->moves[count]);



        //increment ply
        ply++;

        //make sure to make only legal moves
        if( make_move( move_list->moves[count] , all_moves ) == 0){

            //0 means illegal move

            //decrement ply
            ply--;

            //skip to next move
            continue;
        }


        //increment legal moves (to later use for checkmate)
        legal_moves++;


        //score currennt move
        //alpha=-beta, beta=-alpha for the other side now
        int score = -negamax(-beta, -alpha, depth-1); 
        
        //decrement ply
        ply--;

        //take move back
        take_back();

        //fail-hard beta cutoff
        if(score>=beta) {
            
            
            // --- 3. WRITE FAIL-HIGH TO TT ---          //TRANSPOSITION TABLE
            write_hash_entry(beta, depth, hash_beta, move_list->moves[count]);

            
            return beta; //node (move) fails high
        }

        //found a better move
        if(score > alpha){

            // We found a new best move, so it's an Exact PV node
            hash_flag = hash_exact;              //TRANS. TABLE


            //PV node (move)
            alpha = score;

            //if root move
            if(ply==0){
                //assc best move with best score
                best_sofar = move_list->moves[count];

                best_move = best_sofar;       //moved from bottom to here
            }

        }
    }


    //if dont have any legal_moves
    if( legal_moves==0 ){

        //if king in check then checkmated
        if(in_check){
            
            //mating score
            return -49000 + ply ;
            //doing +ply is important else engine wont be able to find
                //checkmate in higher depths (maybe even 4)
            // +ply enables CLOSEST mate possible

        }
        //if not in check then stalemate
        else{
            //return stalemate score
            return 0;
        }

    }


    // //found better move
    // if(old_alpha != alpha){        //MOVED INSIDE IF(SCORE>ALPHA) code block
    //     //init best move
    //     best_move = best_sofar;
    // }



    // --- 4. WRITE EXACT/FAIL-LOW TO TT ---        //TRANSPOSITION TABLE
    write_hash_entry(alpha, depth, hash_flag, best_sofar);



    //node (move) fails low
    return alpha;

    
}



void search_position(/*int depth*/ int max_depth) {

    // //find best move within a given pos
    // int score = negamax(-50000,+50000,depth);

    // if(best_move){
    //     // best move placeholder
    //     printf("bestmove ");
    //     print_move(best_move);
    //     printf("\n");
    // }

    int score = 0;
    
    // Reset flags for the new search
    stopped = 0;
    nodes = 0;
    best_move = 0;

    ply = 0; // <--- ADD THIS: 
    //Guarantee a fresh start for the NNUE and search tree


    // Iterative Deepening Loop
    for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
        
        // Search the current depth
        score = negamax(-50000, 50000, current_depth);
        
        // If we ran out of time mid-search, the score and best_move 
        // from this specific depth are corrupted. Break out immediately!
        if (stopped == 1) {
            break; 
        }
        

        // --- UCI MATE FORMATTING ---
        // If the score is massive, translate it from centipawns to moves-to-mate
        if (score > 48000) {
            // Engine is delivering checkmate
            int mate_in_moves = (49000 - score + 1) / 2;
            printf("info score mate %d depth %d nodes %llu\n", mate_in_moves, current_depth, nodes);
        } 
        else if (score < -48000) {
            // Engine is getting checkmated
            int mate_in_moves = (49000 + score + 1) / 2;
            printf("info score mate -%d depth %d nodes %llu\n", mate_in_moves, current_depth, nodes);
        } 
        else {
            // Standard evaluation
            // Send feedback to the GUI (Eval, depth, and nodes searched)
            printf("info score cp %d depth %d nodes %llu\n", score, current_depth, nodes);
        }



        // Send feedback to the GUI (Eval, depth, and nodes searched)
        //printf("info score cp %d depth %d nodes %llu\n", score, current_depth, nodes);
        //this UCI feedback handled in above else block
    }



    // --- GUARANTEED MOVE FALLBACK ---
    // If the search was aborted instantly and we somehow have no best move, 
    // simply pick the very first legal move available so we don't freeze the GUI.
    if (best_move == 0) {
        moves move_list[1];
        generate_moves(move_list);
        for (int i = 0; i < move_list->count; i++) {
            
            copy_board();

            // If the move is legal, assign it and break
            if (make_move(move_list->moves[i], all_moves)) {
                best_move = move_list->moves[i];
                take_back();
                break;
            }
        }
    }


    // Print the final best move for the GUI to play
    if (best_move) {
        printf("bestmove ");
        print_move(best_move);

        // ADD THIS: Force Windows to send the text to the GUI immediately
        fflush(stdout);
    }








}



//UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI
//UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI
//UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI
//UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI UCI


// parse user/GUI move string input (e.g. "e7e8q")
int parse_move(const char *move_string)
{
    // create move list instance
    moves move_list[1];
    
    // generate moves
    generate_moves(move_list);
    
    // parse source square
    int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;
    
    // parse target square
    int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;
    
    // loop over the moves within a move list
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {
        // init move
        int move = move_list->moves[move_count];
        
        // make sure source & target squares are available within the generated move
        if (source_square == get_move_source(move) && target_square == get_move_target(move))
        {
            // init promoted piece
            int promoted_piece = get_move_prom(move);
            
            // promoted piece is available
            if (promoted_piece)
            {
                // promoted to queen
                if ((promoted_piece == Q || promoted_piece == q) && move_string[4] == 'q')
                    // return legal move
                    return move;
                
                // promoted to rook
                else if ((promoted_piece == R || promoted_piece == r) && move_string[4] == 'r')
                    // return legal move
                    return move;
                
                // promoted to bishop
                else if ((promoted_piece == B || promoted_piece == b) && move_string[4] == 'b')
                    // return legal move
                    return move;
                
                // promoted to knight
                else if ((promoted_piece == N || promoted_piece == n) && move_string[4] == 'n')
                    // return legal move
                    return move;
                
                // continue the loop on possible wrong promotions (e.g. "e7e8f")
                continue;
            }
            
            // return legal move
            return move;
        }
    }
    
    // return illegal move
    return 0;
}

/*
    Example UCI commands to init position on chess board
    
    // init start position
    position startpos
    
    // init start position and make the moves on chess board
    position startpos moves e2e4 e7e5
    
    // init position from FEN string
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 
    
    // init position from fen string and make moves on chess board
    position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves e2a6 e8g8
*/

// parse UCI "position" command
void parse_position(const char *command)
{
    // shift pointer to the right where next token begins
    command += 9;
    
    // init pointer to the current character in the command string
    const char *current_char = command;
    
    // parse UCI "startpos" command
    if (strncmp(command, "startpos", 8) == 0)
        // init chess board with start position
        parse_fen(start_position);
    
    // parse UCI "fen" command 
    else
    {
        // make sure "fen" command is available within command string
        current_char = strstr(command, "fen");
        
        // if no "fen" command is available within command string
        if (current_char == NULL)
            // init chess board with start position
            parse_fen(start_position);
            
        // found "fen" substring
        else
        {
            // shift pointer to the right where next token begins
            current_char += 4;
            
            // init chess board with position from FEN string
            parse_fen(current_char);
        }
    }
    
    // parse moves after position
    current_char = strstr(command, "moves");
    
    // moves available
    if (current_char != NULL)
    {
        // shift pointer to the right where next token begins
        current_char += 6;
        
        // loop over moves within a move string
        while(*current_char)
        {
            // parse next move
            int move = parse_move(current_char);
            
            // if no more moves
            if (move == 0)
                // break out of the loop
                break;
            
            // make move on the chess board
            make_move(move, all_moves);
            
            // move current character mointer to the end of current move
            while (*current_char && *current_char != ' ') current_char++;
            
            // go to the next move
            current_char++;
        }        
    }


// --- NNUE ACCUMULATOR REFRESH ---
    // Ensure the root node's accumulator represents the newly parsed board state
    RefreshAccumulator(acc_stack[0]/*,bitboards*/);


    
    // print board
    //print_board();   //commented due to UCI
}

/*
    Example UCI commands to make engine search for the best move
    
    // fixed depth search
    go depth 64

*/

// parse UCI "go" command
void parse_go(const char *command)
{
    // // init depth
    // int depth = -1;
    
    // // init character pointer to the current depth argument
    // const char *current_depth = NULL;
    
    // // handle fixed depth search
    // if (current_depth = strstr(command, "depth"))
    //     //convert string to integer and assign the result value to depth
    //     depth = atoi(current_depth + 6);
    
    // // different time controls placeholder
   

    // // Fixed time search (e.g., "go movetime 1000" = 1 second)
    // if ((current_depth = strstr(command, "movetime"))) {
    //     int movetime = atoi(current_depth + 9);
        
    //     time_set = 1;
    //     stop_time = get_time_ms() + movetime;
        
    //     // Set depth arbitrarily high (64) so iterative deepening 
    //     // keeps running until the exact millisecond the time expires.
    //     depth = 64; 
    // }
    
    // // Default fallback
    // if (depth == -1) depth = 6;

    
    
    // // search position
    // search_position(depth);


    int depth = -1;
    time_set = 0;
    stopped = 0;
    
    const char *argument = NULL;
    
    // 1. Fixed depth search (e.g., "go depth 6")
    if ((argument = strstr(command, "depth"))) {
        depth = atoi(argument + 6);
    }
    
    // 2. Fixed time per move (e.g., "go movetime 1000")
    if ((argument = strstr(command, "movetime"))) {
        int movetime = atoi(argument + 9);
        time_set = 1;
        stop_time = get_time_ms() + movetime;
        depth = 64; 
    }

    // 3. Standard GUI Tournament Time Controls (e.g., "go wtime 300000 btime 300000")
    int time_remaining = -1;
    
    // Check which side is moving and extract the correct clock
    if (side == white && (argument = strstr(command, "wtime"))) {
        time_remaining = atoi(argument + 6);
    } else if (side == black && (argument = strstr(command, "btime"))) {
        time_remaining = atoi(argument + 6);
    }

    // If we successfully found tournament time, calculate how long to think
    if (time_remaining != -1) {
        time_set = 1;
        
        // Allocate roughly 1/30th of the remaining time for this move
        int allocated_time = time_remaining / 30; 
        
        // Subtract a 50ms safety buffer so the engine never loses on time due to lag
        stop_time = get_time_ms() + allocated_time - 50; 
        
        // Set depth arbitrarily high so iterative deepening runs until time expires
        depth = 64;
    }
    
    // 4. Default fallback if absolutely no time control was understood
    // We lower this to 4 so the engine doesn't freeze for a minute!
    if (depth == -1) {
        depth = 4; 
    }
    
    // Start searching
    search_position(depth);








    
}



/*
    GUI -> isready
    Engine -> readyok
    GUI -> ucinewgame
*/

// main UCI loop
void uci_loop()
{
    // reset STDIN & STDOUT buffers
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    
    // define user / GUI input buffer
    char input[2000];
    
    // print engine info
    printf("id name BBC_modified\n");
    printf("id name CMK_inspired\n");
    printf("uciok\n");
    
    // main loop
    while (1)
    {
        // reset user /GUI input
        memset(input, 0, sizeof(input));
        
        // make sure output reaches the GUI
        fflush(stdout);
        
        // get user / GUI input
        if (!fgets(input, 2000, stdin))
            // continue the loop
            continue;
        
        // make sure input is available
        if (input[0] == '\n')
            // continue the loop
            continue;
        
        // parse UCI "isready" command
        if (strncmp(input, "isready", 7) == 0)
        {
            printf("readyok\n");
            continue;
        }
        
        // parse UCI "position" command
        else if (strncmp(input, "position", 8) == 0)
            // call parse position function
            parse_position(input);
        
        // parse UCI "ucinewgame" command
        else if (strncmp(input, "ucinewgame", 10) == 0) {
            
            clear_hash_table(); // Clear old memory          //TRANSPOSITION TABLE

            // call parse position function
            parse_position("position startpos");
        }
        
        // parse UCI "go" command
        else if (strncmp(input, "go", 2) == 0)
            // call parse go function
            parse_go(input);
        
        // parse UCI "quit" command
        else if (strncmp(input, "quit", 4) == 0)
            // quit from the chess engine program execution
            break;
        
        // parse UCI "uci" command
        else if (strncmp(input, "uci", 3) == 0)
        {
            // print engine info
            printf("id name BBC\n");
            printf("id name Code Monkey King\n");
            printf("uciok\n");
        }
    }
}










//INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL
//INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL
//INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL
//INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL INIT ALL

//init all variables
void init_all(){


    // --- Initialize piece arrays (Fixes C++ initializer error) ---
    memset(char_pieces, 0, sizeof(char_pieces));
    char_pieces['P'] = P; char_pieces['N'] = N; char_pieces['B'] = B; 
    char_pieces['R'] = R; char_pieces['Q'] = Q; char_pieces['K'] = K;
    char_pieces['p'] = p; char_pieces['n'] = n; char_pieces['b'] = b; 
    char_pieces['r'] = r; char_pieces['q'] = q; char_pieces['k'] = k;

    memset(promoted_pieces, 0, sizeof(promoted_pieces));
    promoted_pieces[Q] = 'q'; promoted_pieces[R] = 'r'; promoted_pieces[B] = 'b'; promoted_pieces[N] = 'n';
    promoted_pieces[q] = 'q'; promoted_pieces[r] = 'r'; promoted_pieces[b] = 'b'; promoted_pieces[n] = 'n';
    // -------------------------------------------------------------




    //init leaper pieces attacks
    init_leapers_attacks();

    //init slider pieces attacks
    init_sliders_attacks(bishop);
    init_sliders_attacks(rook);



	//init maagic numbers
	//init_magic_numbers();     //UNCOMMENT OR NOT???????
    //will insert values into the arrays as well

     // init random keys for hashing purposes
    init_random_keys();

    init_hash_table(); // Allocate the TT memory







     // //testing if it works
    // for(int square=0;square<64;square++){
    //     printf(" 0x%llxULL ,\n",rook_magic_numbers[square]);
    // }
    // printf("\n");

    // for(int square=0;square<64;square++){
    //     printf(" 0x%llxULL ,\n",bishop_magic_numbers[square]);
    // }
    // printf("\n");


}




//MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
//MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
//MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN
//MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN MAIN



int main(){

    //init all
    init_all();

    //empty_board "8/8/8/8/8/8/8/8 b - - "
    //start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
    //tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
    //killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
    //cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "

    //parse_fen(tricky_position);
    //print_board();

    //perft_test(6);
    //perft_driver(1);

    LoadNNUE("nnue.bin");

    // //TEMPORARY CODE TO DEBUG WEIGHTS
    // printf("First 5 fc1 weights [0][0..4]:\n");
    // printf("%.6f %.6f %.6f %.6f %.6f\n",
    // FeatureWeights[0][0], FeatureWeights[0][1],
    // FeatureWeights[0][2], FeatureWeights[0][3],
    // FeatureWeights[0][4]);
    // printf("First 5 fc1 biases:\n");
    // printf("%.6f %.6f %.6f %.6f %.6f\n",
    // FeatureBiases[0], FeatureBiases[1],
    // FeatureBiases[2], FeatureBiases[3],
    // FeatureBiases[4]);



    //debug mode variable
    int debug = 0; //0 means debug off means UCI on      //HAVE SET THIS TO 1 TO DEBUG

    //if debugging
    if(debug){
        printf("debug mode\n");

        parse_fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        //parse_fen(start_position);
        
        perft_test(5);









        // // ---> PASTE THE TEST CODE RIGHT HERE <---
        // printf("\n--- SINGLE PIECE TEST ---\n");
        // parse_position("position fen 8/8/8/8/8/8/8/4K3 w - - 0 1");
        // RefreshAccumulator(acc_stack[0]);
        
        // float diff = acc_stack[0].values[0] - FeatureBiases[0];
        
        // printf("acc[0] - bias[0]       = %.6f\n", diff);
        // printf("FeatureWeights[324][0] = %.6f (Claude's expectation)\n", FeatureWeights[324][0]);
        // printf("FeatureWeights[380][0] = %.6f (My expectation)\n", FeatureWeights[380][0]);
        // printf("-------------------------\n\n");




        // // paste this in main() temporarily, after LoadNNUE() is called

        // // Test position: starting position
        // parse_position("position startpos");
        // RefreshAccumulator(acc_stack[0]);

        //     // after RefreshAccumulator(acc_stack[0]);
        //     printf("First 5 accumulator values:\n");
        //     printf("%.6f %.6f %.6f %.6f %.6f\n",
        //     acc_stack[0].values[0], acc_stack[0].values[1],
        //     acc_stack[0].values[2], acc_stack[0].values[3],
        //     acc_stack[0].values[4]);

        // float eval_start = EvaluateNNUE(acc_stack[0]);
        // printf("Starting position:       %+.1f cp\n", (side == white ? eval_start : -eval_start));

        // // Test position: white up a rook (white rook on e1 removed from black)
        // parse_position("position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq - 0 1");
        // RefreshAccumulator(acc_stack[0]);
        // float eval_rook = EvaluateNNUE(acc_stack[0]);
        // printf("White up a rook:         %+.1f cp\n", (side == white ? eval_rook : -eval_rook));

        // // Test position: white up a queen
        // parse_position("position fen rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        // RefreshAccumulator(acc_stack[0]);
        // float eval_queen = EvaluateNNUE(acc_stack[0]);
        // printf("White up a queen:        %+.1f cp\n", (side == white ? eval_queen : -eval_queen));

        // // Test position: king and pawn endgame equal
        // parse_position("position fen 8/4k3/8/8/8/8/4K3/8 w - - 0 1");
        // RefreshAccumulator(acc_stack[0]);
        // float eval_kp = EvaluateNNUE(acc_stack[0]);
        // printf("King pawn endgame equal: %+.1f cp\n", (side == white ? eval_kp : -eval_kp));

        // // Test position: white pawn up
        // parse_position("position fen 8/4k3/8/4P3/8/8/4K3/8 w - - 0 1");
        // RefreshAccumulator(acc_stack[0]);
        // float eval_pawn = EvaluateNNUE(acc_stack[0]);
        // printf("White pawn up:           %+.1f cp\n", (side == white ? eval_pawn : -eval_pawn));


    }
    else{
        //connect to GUI
        uci_loop();
    }

   



    return 0;
}


//not needed now
//loop over 64 squares
    //for(int square =0;square<64;square++){
    //
    //     //print_bitboard(pawn_attacks[white][square]);
    //     //print_bitboard(pawn_attacks[black][square]);
    //     //print_bitboard(knight_attacks[square]);
    //     //print_bitboard(king_attacks[square]);
    //
    //     //print_bitboard(mask_bishop_attacks(square));
    //      //print_bitboard(mask_rook_attacks(square));
    //}