#include <iostream>        
#include <cmath>       
#include <ctime>       
#include <vector>      
#include <pthread.h>   
#include <windows.h>   

using namespace std;


// --- Globals: Config & State ---
vector<vector<int>> matrix;
int rows = 0, cols = 0;     // Main matrix dimensions
int sub_cols = 0, sub_rows = 0; // Task granularity (submatrix size)
int task_index = 0;           // Work queue (next submatrix index)
int total_tasks = 0;          // Total tasks
pthread_mutex_t task_queue_mutex; // Protects task_index (work queue index)

unsigned int seed = 0; // RNG Seed
long long serial_prime_count = 0; // Result (Serial)
long long parallel_prime_count = 0; // Result (Parallel)
int num_threads = 0;
double time_serial = 0.0;     // Benchmarking
double time_parallel = 0.0;   // Benchmarking
pthread_mutex_t prime_count_mutex; // Protects global counter parallel_prime_count


// --- Prototypes ---
void print_menu();
int  is_prime(int n);
void fill_matrix();
void serial_search();
void* worker_routine(void* arg); // Worker thread routine
void parallel_search();

int main(){
    int menu_option;
    int serial_has_run = 0; // Flag to ensure serial baseline is run
    
    // Init sync primitives
    pthread_mutex_init(&task_queue_mutex,NULL);
    pthread_mutex_init(&prime_count_mutex,NULL);


    
    do{ // UI Loop
        print_menu();
        cin >> menu_option;
        switch (menu_option){
        
        case 1: // Set dimensions
            cout << "\nEnter the number of rows: ";
            cin >> rows;
            cout <<"Enter the number of columns: ";
            cin >> cols;
            serial_has_run = 0; // Reset flag if matrix changes
            matrix.assign(rows, vector<int>(cols)); // Allocate matrix
             cout <<"Matrix of " << rows << "x" << cols << " created successfully." << endl;
            break;

        case 2: // Set seed
             cout <<"\nEnter the RNG seed: ";
             cin >> seed;
            srand(seed); 
             cout <<"Seed defined.\n";
            break;

        case 3: // Populate matrix
            
            if(!matrix.empty()){
                fill_matrix();
                 cout <<"Matrix filled successfully.\n";
            }
            else{
                 cout <<"\nERROR: Create the matrix first (Option 1).\n";
            }
            break;
        
        case 4: // Set task granularity
            cout << "Enter the number of submatrix rows: " << endl;
            cin >> sub_rows;
            cout << "Enter the number of submatrix columns: " << endl;
            cin >> sub_cols;
            break;
        case 5: // Set thread pool size
            cout << "Enter the number of threads to use: " << endl;
            cin >> num_threads;
            break;
        case 6: // Run benchmark
            if(!matrix.empty()){
                if(serial_has_run==0){ // Run baseline
                 cout <<"Running serial search...\n";
                serial_search();
                cout <<"Serial search finished.\n";
                serial_has_run=1;
            }
            cout <<"Starting parallel search...\n";
            parallel_search(); // Run parallel test
            } else {
                 cout <<"\nERROR: Create and fill the matrix first.\n";
            }
            
            break;
        case 7: // Display results
            cout << "\n--- Results ---" << endl;
            cout << "Prime count (Serial): " << serial_prime_count << endl;
            cout << "Execution time (Serial): " << time_serial << " seconds." << endl;
            
            cout << "Prime count (Parallel): " << parallel_prime_count << endl;
            cout << "Execution time (Parallel): " << time_parallel << " seconds." << endl;
            cout << "Speed Up: " << time_serial/time_parallel; // Speedup calculation
            break;
        case 8: // Exit
             cout <<"Exiting...\n";
            break;

        default:
             cout <<"Invalid option.\n";
            break;
        }

    }while(menu_option!= 8);

    // Cleanup resources
    pthread_mutex_destroy(&task_queue_mutex);
    pthread_mutex_destroy(&prime_count_mutex);
    
    return 0;
}


// Populate matrix with test data
void fill_matrix(){
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            matrix[i][j] = rand() % 10000000;
        }
    }
}
   
// UI Helper
void print_menu(){
    cout << "\n|\tMenu\t|\n";
    cout << "|\t1-Define matrix size.\n";
    cout << "|\t2-Define RNG seed.\n";
    cout << "|\t3-Fill matrix with random numbers.\n";
    cout << "|\t4-Define submatrix size.\n";
    cout << "|\t5-Define number of threads.\n";
    cout << "|\t6-Run search.\n";
    cout << "|\t7-View execution time and prime count.\n";
    cout << "|\t8-Exit.\n";
    cout << "Choose an option: ";
}

// Optimized primality test
int is_prime(int n){
    if(n <= 1) return 0;
    if(n == 2) return 1;
    if(n % 2 == 0) return 0; // Handle evens
    
    // Check only odds up to sqrt
    for(int i = 3; i <= sqrt(n); i += 2){
        if(n % i == 0){
            return 0;
        }
    }
    return 1;
}

// Single-thread execution (baseline)
void serial_search(){
    serial_prime_count = 0; 
    clock_t start_time, end_time;
    
    start_time = clock();
    for(int i = 0; i < rows; i++){
        for(int j = 0; j < cols; j++){
            serial_prime_count += is_prime(matrix[i][j]);
        }
    }
    end_time = clock();
    time_serial = ((double)(end_time - start_time)) / CLOCKS_PER_SEC; // Store baseline time
}

// Main worker thread routine
void* worker_routine(void* arg) {
    int task_id; // Task ID (submatrix)
    
    int submatrices_per_row = cols / sub_cols;
    int current_sub_row, current_sub_col;
    int row_start, row_end, col_start, col_end;

    // Work loop: consume queue until empty
    while (true) {
        
        // CS: Get next item from work queue
        pthread_mutex_lock(&task_queue_mutex);
        task_id = task_index;
        task_index++;
        pthread_mutex_unlock(&task_queue_mutex);

        if (task_id >= total_tasks) {
            break; // Queue empty, thread exits
        }

        
        // Map 1D task ID to 2D submatrix coordinates
        current_sub_row = task_id / submatrices_per_row;
        current_sub_col = task_id % submatrices_per_row;

        // Calculate submatrix bounds
        row_start = current_sub_row * sub_rows;
        row_end = row_start + sub_rows;
        col_start = current_sub_col * sub_cols;
        col_end = col_start + sub_cols;

        
        // Local processing (avoids mutex contention in inner loop)
        long long local_prime_count = 0;
        for (int i = row_start; i < row_end; i++) {
            for (int j = col_start; j < col_end; j++) {
                local_prime_count += is_prime(matrix[i][j]);
            }
        }
        
        
        // CS: Aggregate local result to global counter
        pthread_mutex_lock(&prime_count_mutex);
        parallel_prime_count += local_prime_count;
        pthread_mutex_unlock(&prime_count_mutex);
    }
    return NULL;
}


// Parallel execution manager
void parallel_search() {
    
    // Reset parallel execution state
    parallel_prime_count = 0;
    task_index = 0; // Reset work queue index
    total_tasks = (rows / sub_rows) * (cols / sub_cols); // Define total task count

    
    vector<pthread_t> threads(num_threads); // Thread pool

    
    clock_t start_time, end_time;
    start_time = clock(); // Start parallel timer

    
    // Dispatch threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_routine, NULL);
    }

    
    // Wait for all threads to complete (join)
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    end_time = clock(); // Stop parallel timer
    time_parallel = ((double)(end_time - start_time)) / CLOCKS_PER_SEC; // Store time
}