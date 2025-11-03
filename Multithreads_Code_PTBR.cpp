#include <iostream>        
#include <cmath>       
#include <ctime>       
#include <vector>      
#include <pthread.h>   
#include <windows.h>   

using namespace std;


// --- Globais de Configuração e Estado ---
vector<vector<int>> matriz;
int lin = 0, col = 0;       // Dimensões da matriz principal
int colSubm = 0,linSubm=0;  // Granularidade da tarefa (tamanho da submatriz)
int iSubm = 0;              // Fila de trabalho (índice da próxima submatriz)
int total_subM =0;         // Total de tarefas
pthread_mutex_t Subm_mutex; // Protege o índice iSubm

unsigned int S = 0; // Seed do RNG
long long contS_primos_total = 0; // Resultado (Serial)
long long contP_primos_total = 0; // Resultado (Paralelo)
int num_threads = 0;
double T_serial = 0.0;     // Benchmarking
double T_paralelo = 0.0;   // Benchmarking
pthread_mutex_t primo_mutex; // Protege o contador global contP_primos_total


// --- Protótipos ---
void imprimir_menu();
int  Ehprimo(int V);
void preenche_matriz();
void busca_serial();
void* rot_thread(void* arg); // Rotina da thread worker
void busca_paralela();

int main(){
    int opcao;
    int buscaS = 0; // Flag para garantir que o baseline serial seja executado
    
    // Inicializa primitivas de sincronização
    pthread_mutex_init(&Subm_mutex,NULL);
    pthread_mutex_init(&primo_mutex,NULL);


    
    do{ // Loop de UI
        imprimir_menu();
        cin >> opcao;
        switch (opcao){
        
        case 1: // Definir dimensões
            cout << "\nInforme a quantidade de linhas: ";
            cin >>lin;
            cout <<"Informe a quantidade de colunas: ";
            cin >> col;
            buscaS = 0; // Reseta flag se a matriz mudar
            matriz.assign(lin, vector<int>(col)); // Aloca matriz
             cout <<"Matriz de " << lin << "x" << col << " criada com sucesso." << endl;
            break;

        case 2: // Definir seed
             cout <<"\nInforme a seed da aleatorização dos numeros: ";
             cin >> S;
            srand(S); 
             cout <<"Semente definida.\n";
            break;

        case 3: // Popular matriz
            
            if(!matriz.empty()){
                preenche_matriz();
                 cout <<"Matriz preenchida com sucesso.\n";
            }
            else{
                 cout <<"\nERRO: Crie a matriz primeiro (Opção 1).\n";
            }
            break;
        
        case 4: // Definir granularidade da tarefa
            cout << "informe a quantidade de linhas da submatriz" << endl;
            cin >> linSubm;
            cout << "informe a quantidade de colunas da submatriz" << endl;
            cin >> colSubm;
            break;
        case 5: // Definir pool de threads
            cout << "informe quantas threads deseja usar" << endl;
            cin >> num_threads;
            break;
        case 6: // Executar benchmark
            if(!matriz.empty()){
                if(buscaS==0){ // Executa baseline
                 cout <<"Executando busca serial\n";
                busca_serial();
                cout <<"Busca serial finalizada.\n";
                buscaS=1;
            }
            cout <<"Iniciando a busca paralela\n";
            busca_paralela(); // Executa teste paralelo
            } else {
                 cout <<"\nERRO: Crie e preencha a matriz primeiro.\n";
            }
            
            break;
        case 7: // Exibir resultados
            cout << "\n--- Resultados ---" << endl;
            cout << "Contagem de primos (Serial): " << contS_primos_total << endl;
            cout << "Tempo de execucao (Serial): " << T_serial << " segundos." << endl;
            
            cout << "Contagem de primos (Paralela): " << contP_primos_total << endl;
            cout << "Tempo de execucao (Paralela): " << T_paralelo << " segundos." << endl;
            cout << "Speed Up de " << T_serial/T_paralelo;
            break;
        case 8: // Sair
             cout <<"Encerrando...\n";
            break;

        default:
             cout <<"Opção inválida.\n";
            break;
        }

    }while(opcao!= 8);

    // Limpeza dos recursos
    pthread_mutex_destroy(&Subm_mutex);
    pthread_mutex_destroy(&primo_mutex);
    
    return 0;
}


// Popula a matriz com dados de teste
void preenche_matriz(){
    for(int i = 0; i < lin; i++){
        for(int j = 0; j < col; j++){
            matriz[i][j] = rand() % 10000000;
        }
    }
}
   
// Helper da UI
void imprimir_menu(){
    cout << "\n|\tMenu de escolhas\t|\n";
    cout << "|\t1-Definir tamanho da matriz.\n";
    cout << "|\t2-Definir semente.\n";
    cout << "|\t3-Preencher a matriz com numeros aleatorios\n";
    cout << "|\t4-Definir o tamanho das submatrizes\n";
    cout << "|\t5-Definir o numero de Threads\n";
    cout << "|\t6-Executar\n";
    cout << "|\t7-Visualizar o tempo de execucao e quantidade de numeros primos\n";
    cout << "|\t8-Encerrar\n";
    cout << "Escolha uma opcao: ";
}

// Teste de primalidade otimizado
int Ehprimo(int V){
    if(V <= 1) return 0;
    if(V == 2) return 1;
    if(V % 2 == 0) return 0; // Trata pares
    
    // Verifica apenas ímpares até a raiz
    for(int i = 3; i <= sqrt(V); i += 2){
        if(V % i == 0){
            return 0;
        }
    }
    return 1;
}

// Execução single-thread (baseline)
void busca_serial(){
    contS_primos_total = 0; 
    clock_t inicio,fim;
    
    inicio = clock();
    for(int i = 0; i < lin; i++){
        for(int j = 0; j < col; j++){
            contS_primos_total += Ehprimo(matriz[i][j]);
        }
    }
    fim = clock();
    T_serial = ((double)(fim - inicio)) / CLOCKS_PER_SEC; // Armazena tempo do baseline
}

// Rotina principal da thread worker
void* rot_thread(void* arg) {
    int trab; // ID da tarefa (submatriz)
    
    int submatrizes_por_linha = col / colSubm;
    int sub_linha_atual, sub_col_atual;
    int inicio_lin, fim_lin, inicio_col, fim_col;

    // Loop de trabalho: consome da fila até acabar
    while (true) {
        
        // CS: Pega o próximo item da fila de trabalho
        pthread_mutex_lock(&Subm_mutex);
        trab = iSubm;
        iSubm++;
        pthread_mutex_unlock(&Subm_mutex);

        if (trab >= total_subM) {
            break; // Fila vazia, encerra a thread
        }

        
        // Mapeia o ID 1D da tarefa para coordenadas 2D da submatriz
        sub_linha_atual = trab / submatrizes_por_linha;
        sub_col_atual = trab % submatrizes_por_linha;

        // Calcula limites da submatriz
        inicio_lin = sub_linha_atual * linSubm;
        fim_lin = inicio_lin + linSubm;
        inicio_col = sub_col_atual * colSubm;
        fim_col = inicio_col + colSubm;

        
        // Processamento local (evita contenção de mutex no loop interno)
        long long primos_locais = 0;
        for (int i = inicio_lin; i < fim_lin; i++) {
            for (int j = inicio_col; j < fim_col; j++) {
                primos_locais += Ehprimo(matriz[i][j]);
            }
        }
        
        
        // CS: Agrega o resultado local ao contador global
        pthread_mutex_lock(&primo_mutex);
        contP_primos_total += primos_locais;
        pthread_mutex_unlock(&primo_mutex);
    }
    return NULL;
}


// Gerenciador da execução paralela
void busca_paralela() {
    
    // Reseta estado da execução paralela
    contP_primos_total = 0;
    iSubm = 0;
    total_subM = (lin / linSubm) * (col / colSubm); // Define o total de tarefas

    
    vector<pthread_t> threads(num_threads); // Pool de threads

    
    clock_t inicio, fim;
    inicio = clock(); // Inicia timer paralelo

    
    // Dispara as threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, rot_thread, NULL);
    }

    
    // Aguarda a conclusão de todas as threads (join)
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    fim = clock(); // Para timer paralelo
    T_paralelo = ((double)(fim - inicio)) / CLOCKS_PER_SEC; // Armazena tempo
}