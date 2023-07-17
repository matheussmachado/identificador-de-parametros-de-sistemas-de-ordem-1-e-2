# IDENTIFICADOR PARÂMETROS DE SISTEMAS DE 1º E 2º ORDEM

## Sobre

O projeto foi realizado para um trabalho de avaliação da disciplina de Controle Digital.
  
Foi implementado um algoritmo de mínimos quadrados para a identificação de parâmetros que descrevem o modelo matemático de sistemas lineares de ordem 1 e ordem 2. O algoritmo está sendo executado através de um microcontrolador.  
  
Materiais do projeto:  
- 1 microcontrolador ATmega328p (Arduino Uno) que foi programado utilizando a lib AVR  
- 1 filtro passa-baixas passivo RC de ordem 1  
- 2 filtros passa-baixas passivos RC de ordem 2 com parâmetros diferentes de resistência  
- 1 filtro ativo passa-baixas butterworth (topologia Sallen-Key) de ordem 2

Inicialmente, o projeto previsto para identificar apenas o filtro do tipo Butterworth de ordem 2, entretanto foi montado outros circuitos para validar o algoritmo. Sobre o filtro Butterworth, ele foi projetado para ter uma frequência de corte em 10 Hz e amortecimento 0,707. 

 A seguir, mais detalhes sobre a implementação do algoritmo e circuitos utilizados.

##  O algoritmo de identificação dos parâmetros

O algoritmo consiste na aplicação do princípio de mínimos quadrados, em que dada as entradas conhecidas que foram inseridas no sistema e obtidas as saídas para cada uma dessas entradas é possível montar uma matriz de regressores $\Phi$ com a finalidade de aplicar o método de mínimos quadrados e obter os parâmetros que descrevem a dinâmica do sistema.

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/59ddb5c6-c3c2-4d6c-85f9-4930261e88c6)

Nesse contexto, $\hat{y}$ é a saída estimada do sistema, os parâmetros $\theta$ são os parâmetros a serem identificados e por fim as variáveis $\varphi$ estão relacionadas às entradas e saídas anteriores.

A mesma equação pode ser escrita na forma:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/d8030eb2-e494-486f-afd4-e00068f7a758)

Portanto, os parâmetros estimados podem ser obtidos através de:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/ee33bbc9-aa74-4892-aa1c-38aa1d61d908)

Para um sistema de ordem 1 no tempo discreto, é possível escrevê-lo como:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/13c40204-0ab8-4f17-a3bf-6873b6032b80)

O formato das matrizs $y$ e $\Phi$ , com um número N de amostras coletadas em um ensaio é da forma:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/abe169ef-5040-45cc-be24-31c7b99baa2c)

Portanto, a montagem das matrizes para o cálculo dos parâmetros $\theta$ fica:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/899a61ab-6f21-4850-868c-dbac7995884e)

Em C, é necessário realizar uma adaptação, pois a indexação inicia em zero. Para um total de 50 amostras coletadas, temos $N = 49$

Desta forma, para fins de generalização para a coleta de amostras, deve-se atualizar o vetor de amostras de entradas e saídas até um número $N - n - 1$ em que $n$ é a ordem do sistema.

Nesse contexto, levando em conta as adaptações, para um sistema de ordem 2:

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/ee282c27-9404-48d5-8412-55125675afcf)

![image](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/0f775bca-9715-4a91-886d-256a950f5f2b)

## Sobre a escolha do período de amostragem

O período de amostragem foi escolhido com base no critério do tempo de subida do sistema. A razão entre tempo de subida do sistema e o período de amostragem deve ser entre 4 e 10, em quanto maior a razão, melhor é captura da dinâmica do sistema amostrado. O tempo de subida considerado foi o do filtro ativo Butterwoth de segunda ordem. Assim, foi utilizado o mesmo período de amostragem para os demais sistemas.

## Resultados

Será detalhado o processo para o filtro do tipo Butterworth e o restante dos sistemas serão apenas demonstrados os resultados.

### Filtro passa-baixas ativo do tipo Butterworth (2º Ordem)

Função de transferência do sistema em tempo contínuo:

![ft-butterworth-ord2](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/67f07e75-5d92-4ce9-a5c7-a278b3754ffb)


Esquemático do circuito e montagem física:

![mix-butterworth-ord2](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/15cfd42d-96cf-404f-b510-f6358f77e063)


Ensaio para obtenção dos vetores de entradas e saídas:

![ensaio-prbs](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/61b258f8-32f8-4538-84c8-3c2be49b279a)

É realizado um ensaio utilizando como entrada (em azul) um sinal do tipo PRBS (Pseudo Random Binary Signal) de quatro bits. A cada período de amostragem é que coleta os dados de entrada e de saída real do sistema (em vermelho). Após essa captura é realizado o cálculo de identificação dos parâmetros do sistema por mínimos quadrados e dado uma pausa para fins de visualização. Em seguida é alterada entre quatro tipos de entradas distintas, via acionamento de um botão físico, para verificar se a saída estimada (em verde) consegue prevê a saída real do sistema em função dos parâmetros identificados e das entradas inseridas.


Resposta ao sinal PRBS com espaçamento:

![rastreando-prbs](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/ea8053bd-3fb5-474a-9d19-2af87d47c445)


Resposta ao sinal degrau:

![rastreando-degrau](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/1585f113-a051-4d01-94fc-cc2b8fc97aa3)


Resposta ao sinal senoidal:

![rastreando-senoide](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/72c1ada4-3c92-4e1c-a652-42f1af872a1b)


Resposta ao sinal rampa:

![rastreando-rampa](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/f06e127c-ea7f-4cb6-bd1b-8fb7d4d54d18)


Interação em tempo real:

https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/e24e0172-7f55-471c-8e92-7be0d8db0d85


### Filtro passa-baixas passivo do tipo RC (2º Ordem)

Função de transferência do sistema em tempo contínuo:

![ft-passivo-ord2](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/3f26f501-c3a9-4e91-932f-46f5a45c9222)

#### Sistema 1 (mais rápido)

Esquemático do circuito e montagem física:

![mix-passivo-ord2-rapido](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/1992f64c-13ed-4b51-a9bb-83dbbe71f586)


Interação em tempo real:

https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/7ff2c260-1a01-419d-a68d-762bd4abd76f


#### Sistema 2 (mais lento)

Esquemático do circuito e montagem física:

![mix-passivo-ord2-lento](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/e5775da3-89da-4bdf-a256-2c5368579486)


Interação em tempo real:


https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/5c4971c6-3806-4329-af62-1e6b3e4338a2


### Filtro passa-baixas passivo do tipo RC (1º Ordem)

Função de transferência do sistema em tempo contínuo:

![ft-passivo-ord1](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/83671bdb-3619-46af-b37e-1341a917d271)


Esquemático do circuito e montagem física:

![mix-passivo-ord1](https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/9296a34b-cc41-41b1-aafc-a13b427e652f)


Interação em tempo real:

https://github.com/matheussmachado/identificador-de-parametros-de-sistemas-de-ordem-1-e-2/assets/63216146/9095c000-baab-4e98-b770-4e7b43f7c823
