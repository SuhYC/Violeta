# Violeta

## Skills
Server : cpp IOCP Server </br>
Client : UnityC#</br>
SHA-256 : openssl on cpp, System.Security.Cryptography on C#</br>
S-box : Rijndael Substitution Box (AES - SubBytes)

## 목표
네트워크 패킷 스니핑과 상관없는 비올레타 검증 시스템 만들기

## 원리
동일한 시드로부터 동일한 결과를 낼 수 있도록 유도한다. </br>
시드값은 현재시간으로 가정한다. (클라이언트의 PC시간이 잘못되었을 경우엔 서버와의 시간차를 미리 기록해두는 것이 좋을 것 같다.) </br>

비올레타 탐지 시스템의 목적은 1명의 인원이 다수의 컴퓨터를 동작하는 경우를 탐지하기 위해서로 가정한다. </br>
이 때, 다수의 클라이언트가 비슷한 시드값을 선택하더라도 결과에 큰 차이를 만들기 위해 SHA-256을 사용하여 Avalanche Effect를 유도한다.</br>
(네트워크 지연시간을 악용해 동일한 시드값을 지정할 수도 있으므로 현재시간시드값의 나머지값을 제약조건으로 걸 수도 있다.)

역어셈블리의 내성을 증가시키기 위해 S-box연산을 적용한다. </br>
SHA-256을 통해 나온 결과값 32바이트를 각 1바이트마다 치환연산을 적용한다.</br>
이 S-box는 코드에는 AES의 S-Box를 적용했지만 임의의 S-box로 변경하여도 된다. (클라이언트와 서버의 S-box가 동일하기만 하면 된다.

현재시간 -> SHA-256 -> Sbox를 통해 나온 결과값으로 비올레타의 움직임을 재생한 이후 </br>
유저의 입력값과 처음에 사용한 시간시드값을 서버로 전송한다.</br>
서버는 동일한 시간시드값을 바탕으로 시뮬레이션한 후, 적절한 입력이 맞는지 확인한다.

## 영상
[![Video Label](http://img.youtube.com/vi/EYDAL4GmfnE/0.jpg)](https://youtu.be/EYDAL4GmfnE)
