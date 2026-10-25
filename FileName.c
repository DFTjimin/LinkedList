#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>

#define IMAGE_SIZE 28
#define FILTER_SIZE 3
#define FILTER_COUNT 8
#define CONV_SIZE (IMAGE_SIZE - FILTER_SIZE + 1)
#define POOL_SIZE (CONV_SIZE / 2)
#define FEATURE_COUNT (FILTER_COUNT * POOL_SIZE * POOL_SIZE)

#define TRAIN_IMAGE_FILE "emnist-byclass-train-images-idx3-ubyte"
#define TRAIN_LABEL_FILE "emnist-byclass-train-labels-idx1-ubyte"

// 학습 데이터
typedef struct ImageDataset {
    unsigned char* pixels;
    unsigned char* labels;
    int sampleCount;
} ImageDataset;

// 가중치와 편향
typedef struct CNNModel {
    double filterWeights[FILTER_COUNT][FILTER_SIZE][FILTER_SIZE];
    double filterBiases[FILTER_COUNT];
    double denseWeights[52][FEATURE_COUNT];
    double denseBiases[52];
} CNNModel;

//순전파 결과
typedef struct ForwardResult {
    double inputImage[IMAGE_SIZE][IMAGE_SIZE];
    double convolutionOutput[FILTER_COUNT][CONV_SIZE][CONV_SIZE];
    double reluOutput[FILTER_COUNT][CONV_SIZE][CONV_SIZE];
    double poolingOutput[FILTER_COUNT][POOL_SIZE][POOL_SIZE];
    double flattenedFeatures[FEATURE_COUNT];
    double alphabetScores[52];
    double alphabetProbabilities[52];
    double probabilitySum;
    int predictedAlphabetIndex;
    double highestProbability;
} ForwardResult;

const char alphabetNames[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// 함수 원형
int imageLoad(ImageDataset* dataset);
void initializeModel(CNNModel* model);
void prepareInputImage(ImageDataset* dataset, int imageIndex, ForwardResult* result);
void forward(CNNModel* model, ForwardResult* result);
void backward(CNNModel* model, ForwardResult* result, int targetAlphabetIndex, double learningRate);
int trainModel(CNNModel* model, ImageDataset* dataset, ForwardResult* result);
void predictFile(CNNModel* model, ForwardResult* result);

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    ImageDataset trainFile = { 0 };
    static CNNModel model;
    static ForwardResult result;
    int choice = 0;
    int modelTrained = 0;
    //가중치초기화
    initializeModel(&model);
//메뉴
    while (1) {
        printf("\n_______________알파벳 CNN_____________\n");
        printf("1. 학습\n");
        printf("2. TXT 파일 한장 예측\n");
        printf("선택: ");
        scanf("%d", &choice);
        switch (choice) {
        case 1:
            if (trainModel(&model, &trainFile, &result)) {
                modelTrained = 1;
            }
            break;

        case 2:
            if (modelTrained) {
                predictFile(&model, &result);
            }
            else {
                printf("먼저 1번에서 학습하세요.\n");
            }
            break;
        }
    }

    free(trainFile.pixels);
    free(trainFile.labels);
    return 0;
}

int imageLoad(ImageDataset* dataset)
{
    FILE* imageFile = fopen(TRAIN_IMAGE_FILE, "rb");
    FILE* labelFile = fopen(TRAIN_LABEL_FILE, "rb");
    unsigned char header[16];
    unsigned int count = 0;
    size_t pixelCount;
    size_t pixelsRead;
    size_t labelsRead;
    int index;
    // 헤더를 읽고 이미지 수를 구한다.
    fread(header, 1, 16, imageFile);

    for (index = 4; index < 8; index++) {
        count = count * 256 + header[index];
    }

    // 이미지 픽셀과 정답을 저장할 공간을 확보한다.
    pixelCount = (size_t)count * IMAGE_SIZE * IMAGE_SIZE;
    dataset->pixels = malloc(pixelCount);
    dataset->labels = malloc(count);

    // 정답 파일의 헤더를 건너뛰고 이미지와 정답을 읽는다.
    fseek(labelFile, 8, SEEK_SET);
    pixelsRead = fread(dataset->pixels, 1, pixelCount, imageFile);
    labelsRead = fread(dataset->labels, 1, count, labelFile);
    fclose(imageFile);
    fclose(labelFile);
    dataset->sampleCount = (int)count;
    printf("읽은 이미지 수: %d\n", dataset->sampleCount);
    return 1;
}

void initializeModel(CNNModel* model)
{
    int filterIndex;
    int filterRow;
    int filterColumn;
    int classIndex;
    int featureIndex;
    double randomValue;

    //가중치는난수 편향은 0
    srand(1);
    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        model->filterBiases[filterIndex] = 0.0;
        for (filterRow = 0; filterRow < FILTER_SIZE; filterRow++) {
            for (filterColumn = 0; filterColumn < FILTER_SIZE; filterColumn++) {
                randomValue = (double)rand() / RAND_MAX;
                model->filterWeights[filterIndex][filterRow][filterColumn] =
                    (randomValue * 2.0 - 1.0) * 0.1;
            }
        }
    }

    for (classIndex = 0; classIndex < 52; classIndex++) {
        model->denseBiases[classIndex] = 0.0;
        for (featureIndex = 0; featureIndex < FEATURE_COUNT; featureIndex++) {
            randomValue = (double)rand() / RAND_MAX;
            model->denseWeights[classIndex][featureIndex] =
                (randomValue * 2.0 - 1.0) * 0.1;
        }
    }
}

//이미지 방향보정(mnist 이미지 특성), 255로 나눠 0~1값으로 만들기
void prepareInputImage(ImageDataset* dataset, int imageIndex, ForwardResult* result)
{
    int row;
    int column;
    size_t imageStart = (size_t)imageIndex * IMAGE_SIZE * IMAGE_SIZE;
    for (row = 0; row < IMAGE_SIZE; row++) {
        for (column = 0; column < IMAGE_SIZE; column++) {
            result->inputImage[row][column] = dataset->pixels[imageStart + column * IMAGE_SIZE + row] / 255.0;
        }
    }
}

//순전파
void forward(CNNModel* model, ForwardResult* result)
{
    int filterIndex;
    int filterRow;
    int filterColumn;
    int row;
    int column;
    int classIndex;

    //합성곱 3x3필터 8개로 특징 계산
    double convolutionSum;

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        for (row = 0; row < CONV_SIZE; row++) {
            for (column = 0; column < CONV_SIZE; column++) {
                convolutionSum = model->filterBiases[filterIndex];
                for (filterRow = 0; filterRow < FILTER_SIZE; filterRow++) {
                    for (filterColumn = 0; filterColumn < FILTER_SIZE; filterColumn++) {
                        convolutionSum += result->inputImage[row + filterRow][column + filterColumn] * model->filterWeights[filterIndex][filterRow][filterColumn];
                    }
                }
                result->convolutionOutput[filterIndex][row][column] = convolutionSum;
            }
        }
    }

    //결과값의 음수를 0으로 만들기
    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {

        for (row = 0; row < CONV_SIZE; row++) {
            for (column = 0; column < CONV_SIZE; column++) {
                if (result->convolutionOutput[filterIndex][row][column] < 0.0) {
                    result->reluOutput[filterIndex][row][column] = 0.0;
                }
                else {
                    result->reluOutput[filterIndex][row][column] = result->convolutionOutput[filterIndex][row][column];
                }
            }
        }
    }

    //2x2영역마다 최댓값만 남기기 (최대풀링)
    int windowRow;
    int windowColumn;
    double maximumValue;
    double currentValue;

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        for (row = 0; row < POOL_SIZE; row++) {
            for (column = 0; column < POOL_SIZE; column++) {
                maximumValue = result->reluOutput[filterIndex][row * 2][column * 2];
                for (windowRow = 0; windowRow < 2; windowRow++) {
                    for (windowColumn = 0; windowColumn < 2; windowColumn++) {

                        currentValue = result->reluOutput[filterIndex][row * 2 + windowRow] [column * 2 + windowColumn];

                        if (currentValue > maximumValue) {
                            maximumValue = currentValue;
                        }
                    }
                }
                result->poolingOutput[filterIndex][row][column] = maximumValue;
            }
        }
    }

    //1352개의 배열에 결과를 펼치기 (일차원뱌열)
    int featureIndex = 0;
    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        for (row = 0; row < POOL_SIZE; row++) {
            for (column = 0; column < POOL_SIZE; column++) {
                result->flattenedFeatures[featureIndex] =
                    result->poolingOutput[filterIndex][row][column];
                featureIndex++;
            }
        }
    }

    //특징값으로 점수구하기
    double scoreSum;

    for (classIndex = 0; classIndex < 52; classIndex++) {
        scoreSum = model->denseBiases[classIndex];
        for (featureIndex = 0; featureIndex < FEATURE_COUNT; featureIndex++) {
            scoreSum +=result->flattenedFeatures[featureIndex] * model->denseWeights[classIndex][featureIndex];
        }
        result->alphabetScores[classIndex] = scoreSum;
    }

    //점수를 확률로 변경
    double exponentialScores[52];
    double maximumScore = result->alphabetScores[0];
    double exponentialSum = 0.0;
    result->probabilitySum = 0.0;

    for (classIndex = 1; classIndex < 52; classIndex++) {

        if (result->alphabetScores[classIndex] > maximumScore) {
            maximumScore = result->alphabetScores[classIndex];
        }
    }

    for (classIndex = 0; classIndex < 52; classIndex++) {
        exponentialScores[classIndex] = exp(result->alphabetScores[classIndex] - maximumScore);
        exponentialSum += exponentialScores[classIndex];
    }
    for (classIndex = 0; classIndex < 52; classIndex++) {

        result->alphabetProbabilities[classIndex] = exponentialScores[classIndex] / exponentialSum;
        result->probabilitySum += result->alphabetProbabilities[classIndex];
    }
    //가장높은 확률 알파벳이 예측알파벳
    result->predictedAlphabetIndex = 0;
    result->highestProbability = result->alphabetProbabilities[0];

    for (classIndex = 1; classIndex < 52; classIndex++) {
        if (result->alphabetProbabilities[classIndex] > result->highestProbability) {
            result->highestProbability = result->alphabetProbabilities[classIndex];
            result->predictedAlphabetIndex = classIndex;
        }
    }
}

//역전파
void backward(CNNModel* model, ForwardResult* result, int targetAlphabetIndex, double learningRate)
{
    int classIndex;
    int featureIndex;
    int filterIndex;
    int filterRow;
    int filterColumn;
    int row;
    int column;
    int windowRow;
    int windowColumn;
    double maximumValue;
    double currentValue;

    double scoreGradients[52];

    for (classIndex = 0; classIndex < 52; classIndex++) {
        scoreGradients[classIndex] = result->alphabetProbabilities[classIndex];
    }

    scoreGradients[targetAlphabetIndex] = scoreGradients[targetAlphabetIndex] - 1.0;


    static double denseWeightGradients[52][FEATURE_COUNT];
    double denseBiasGradients[52];
    for (classIndex = 0; classIndex < 52; classIndex++) {
        denseBiasGradients[classIndex] = scoreGradients[classIndex];

        for (featureIndex = 0; featureIndex < FEATURE_COUNT; featureIndex++) {
            denseWeightGradients[classIndex][featureIndex] = scoreGradients[classIndex] * result->flattenedFeatures[featureIndex];
        }
    }
    double featureGradients[FEATURE_COUNT];

    for (featureIndex = 0; featureIndex < FEATURE_COUNT; featureIndex++) {
        featureGradients[featureIndex] = 0.0;
        for (classIndex = 0; classIndex < 52; classIndex++) {
            featureGradients[featureIndex] += scoreGradients[classIndex] * model->denseWeights[classIndex][featureIndex];
        }
    }

    double poolingGradients[FILTER_COUNT][POOL_SIZE][POOL_SIZE];

    featureIndex = 0;

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {

        for (row = 0; row < POOL_SIZE; row++) {

            for (column = 0; column < POOL_SIZE; column++) {
                poolingGradients[filterIndex][row][column] = featureGradients[featureIndex];

                featureIndex++;
            }
        }
    }

    double reluGradients[FILTER_COUNT][CONV_SIZE][CONV_SIZE];

    int maximumRow;
    int maximumColumn;
    int currentRow;
    int currentColumn;
    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {

        for (row = 0; row < CONV_SIZE; row++) {

            for (column = 0; column < CONV_SIZE; column++) {
                reluGradients[filterIndex][row][column] = 0.0;
            }
        }
    }

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {

        for (row = 0; row < POOL_SIZE; row++) {

            for (column = 0; column < POOL_SIZE; column++) {
                maximumRow = row * 2;
                maximumColumn = column * 2;
                maximumValue =
                    result->reluOutput[filterIndex][maximumRow][maximumColumn];
                for (windowRow = 0; windowRow < 2; windowRow++) {
                    for (windowColumn = 0; windowColumn < 2; windowColumn++) {
                        currentRow = row * 2 + windowRow;
                        currentColumn = column * 2 + windowColumn;
                        currentValue = result->reluOutput[filterIndex][currentRow][currentColumn];

                        if (currentValue > maximumValue) {
                            maximumValue = currentValue;
                            maximumRow = currentRow;
                            maximumColumn = currentColumn;
                        }
                    }
                }
                reluGradients[filterIndex][maximumRow][maximumColumn] += poolingGradients[filterIndex][row][column];
            }
        }
    }

    double convolutionGradients[FILTER_COUNT][CONV_SIZE][CONV_SIZE];
    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {

        for (row = 0; row < CONV_SIZE; row++) {

            for (column = 0; column < CONV_SIZE; column++) {

                if (result->convolutionOutput[filterIndex][row][column] > 0.0) {
                    convolutionGradients[filterIndex][row][column] = reluGradients[filterIndex][row][column];
                }
                else {
                    convolutionGradients[filterIndex][row][column] = 0.0;
                }
            }
        }
    }
    double filterWeightGradients[FILTER_COUNT][FILTER_SIZE][FILTER_SIZE];
    double filterBiasGradients[FILTER_COUNT];
    double currentConvolutionGradient;

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        filterBiasGradients[filterIndex] = 0.0;

        for (filterRow = 0; filterRow < FILTER_SIZE; filterRow++) {
            for (filterColumn = 0; filterColumn < FILTER_SIZE; filterColumn++) {
                filterWeightGradients[filterIndex][filterRow][filterColumn] = 0.0;
            }
        }

        for (row = 0; row < CONV_SIZE; row++) {

            for (column = 0; column < CONV_SIZE; column++) {

                currentConvolutionGradient =
                    convolutionGradients[filterIndex][row][column];
                filterBiasGradients[filterIndex] += currentConvolutionGradient;

                for (filterRow = 0; filterRow < FILTER_SIZE; filterRow++) {

                    for (filterColumn = 0; filterColumn < FILTER_SIZE; filterColumn++) {
                        filterWeightGradients[filterIndex][filterRow][filterColumn] += currentConvolutionGradient * result->inputImage[row + filterRow][column + filterColumn];
                    }
                }
            }
        }
    }

    for (classIndex = 0; classIndex < 52; classIndex++) {

        model->denseBiases[classIndex] = model->denseBiases[classIndex] - learningRate * denseBiasGradients[classIndex];

        for (featureIndex = 0; featureIndex < FEATURE_COUNT; featureIndex++) {
            model->denseWeights[classIndex][featureIndex] = model->denseWeights[classIndex][featureIndex] - learningRate * denseWeightGradients[classIndex][featureIndex];
        }
    }

    for (filterIndex = 0; filterIndex < FILTER_COUNT; filterIndex++) {
        model->filterBiases[filterIndex] =
            model->filterBiases[filterIndex] - learningRate * filterBiasGradients[filterIndex];
        for (filterRow = 0; filterRow < FILTER_SIZE; filterRow++) {
            for (filterColumn = 0; filterColumn < FILTER_SIZE; filterColumn++) {
                model->filterWeights[filterIndex][filterRow][filterColumn] = model->filterWeights[filterIndex][filterRow][filterColumn] - learningRate * filterWeightGradients[filterIndex][filterRow][filterColumn];
            }
        }
    }

}


//에포크 수만큼 반복
int trainModel(CNNModel* model, ImageDataset* dataset, ForwardResult* result)
{
    int* imageOrder;
    int alphabetCount = 0;
    int trainingCount;
    int epochCount;
    int epoch;
    int imageIndex;
    int orderIndex;
    int randomIndex;
    int temporaryIndex;
    int targetAlphabetIndex;
    int alphabetIndex;
    unsigned int randomValue;
    double learningRate = 0.005;
    double totalLoss;
    double maximumScore;
    double exponentialSum;

    if (dataset->pixels == NULL) {
        if (!imageLoad(dataset)) {
            return 0;
        }
    }

    //라벨이 10~61인 알파벳 수
    for (imageIndex = 0; imageIndex < dataset->sampleCount; imageIndex++) {
        if (dataset->labels[imageIndex] >= 10) {
            if (dataset->labels[imageIndex] <= 61) {
                alphabetCount++;
            }
        }
    }

    //반복수 이미지수 입력
    printf("사용 가능한 알파벳: %d장\n", alphabetCount);
    printf("에포크 수: ");
    scanf("%d", &epochCount);
    if (epochCount < 1) {
        printf("에포크는 1 이상으로 입력하세요.\n");
        return 0;
    }

    printf("학습할 알파벳수 (0=전체): ");
    scanf("%d", &trainingCount);
    if (trainingCount < 0) {
        printf("학습 장수를 확인하세요.\n");
        return 0;
    }

    if (trainingCount > alphabetCount) {
        printf("학습 장수를 확인하세요.\n");
        return 0;
    }

    if (trainingCount == 0) {
        trainingCount = alphabetCount;
    }

    imageOrder = malloc((size_t)trainingCount * sizeof(int));

    orderIndex = 0;
    for (imageIndex = 0; imageIndex < dataset->sampleCount; imageIndex++) {
        if (orderIndex >= trainingCount) {
            break;
        }
        if (dataset->labels[imageIndex] >= 10) {
            if (dataset->labels[imageIndex] <= 61) {
                imageOrder[orderIndex] = imageIndex;
                orderIndex++;
            }
        }
    }

    printf("학습: %d장 x %d에포크 | 학습률: %f\n", trainingCount, epochCount, learningRate);

    for (epoch = 0; epoch < epochCount; epoch++) {
        totalLoss = 0.0;

        //에포크마다 이미지순서 섞기
        for (orderIndex = trainingCount - 1; orderIndex > 0; orderIndex--) {
            randomValue = (unsigned int)rand();
            randomValue = randomValue * 32768u + (unsigned int)rand();
            randomIndex = (int)(randomValue % (unsigned int)(orderIndex + 1));

            temporaryIndex = imageOrder[orderIndex];
            imageOrder[orderIndex] = imageOrder[randomIndex];
            imageOrder[randomIndex] = temporaryIndex;
        }

        for (orderIndex = 0; orderIndex < trainingCount; orderIndex++) {
            imageIndex = imageOrder[orderIndex];
            targetAlphabetIndex = dataset->labels[imageIndex] - 10;

            //이미지1장 순전파+손실계산 후 역전파 실행
            prepareInputImage(dataset, imageIndex, result);
            forward(model, result);

            maximumScore = result->alphabetScores[0];
            for (alphabetIndex = 1; alphabetIndex < 52; alphabetIndex++) {
                if (result->alphabetScores[alphabetIndex] > maximumScore) {
                    maximumScore = result->alphabetScores[alphabetIndex];
                }
            }

            exponentialSum = 0.0;
            for (alphabetIndex = 0; alphabetIndex < 52; alphabetIndex++) {
                exponentialSum += exp(result->alphabetScores[alphabetIndex] - maximumScore);
            }

            totalLoss += (maximumScore - result->alphabetScores[targetAlphabetIndex]) + log(exponentialSum);
            backward(model, result, targetAlphabetIndex, learningRate);

            //1000번마다 or 마지막한번때 출력
            if ((orderIndex + 1) % 1000 == 0) {
                printf("\rEpoch %d/%d | %d/%d장", epoch + 1, epochCount, orderIndex + 1, trainingCount);
                fflush(stdout);
            }
            else if (orderIndex + 1 == trainingCount) {
                printf("\rEpoch %d/%d | %d/%d장", epoch + 1, epochCount, orderIndex + 1, trainingCount);
                fflush(stdout);
            }
        }

        printf(" | 평균 학습 손실: %f\n", totalLoss / trainingCount);
    }

    free(imageOrder);
    printf("학습 완료.\n");
    return 1;
}
//파일의ㅏ 픽셀 784개읽고 순전파만 실행해 예측
void predictFile(CNNModel* model, ForwardResult* result)
{
    char filename[260];
    FILE* imageFile;
    int row;
    int column;
    int pixelValue;

    printf("TXT 파일명: ");
    if (scanf("%s", filename) != 1) {
        return;
    }
    imageFile = fopen(filename, "r");
    for (row = 0; row < IMAGE_SIZE; row++) {
        for (column = 0; column < IMAGE_SIZE; column++) {
            fscanf(imageFile, "%d", &pixelValue);
            result->inputImage[row][column] = pixelValue / 255.0;
        }
    }
    fclose(imageFile);
    forward(model, result);

    printf("예측한 알파벳: %c\n", alphabetNames[result->predictedAlphabetIndex]);
    printf("예측 확률: %f%c\n", result->highestProbability * 100.0, '%');
}