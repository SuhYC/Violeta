using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System;
using System.Security.Cryptography;
using System.Text;
using System.Diagnostics.CodeAnalysis;

/*
 * 1. 시간난수 받기
 * 2. 시간난수를 string으로 바꾸어 해시값 만들기
 * 3. byte배열로 바꾸어 sbox연산하기
 * 4. 
*/

public class Test : MonoBehaviour
{
    const float epochTime = 5f;

    long m_time;
    private float m_lv;
    private GameObject RealButtonPanel;
    private Slider TimePanel;
    private float m_timeleft;
    readonly byte[] Sbox = new byte[]
    {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
    };

    Violeta[] m_violeta = new Violeta[4];

    private int[] m_answer; // 0 = null, 1~4 = answered
    private int m_idxCount;

    void OnEnable()
    {
        for(int violetaNum = 0; violetaNum < 4; violetaNum++)
        {
            m_violeta[violetaNum] = transform.GetChild(violetaNum).GetComponent<Violeta>();
        }

        m_answer = new int[4];
        m_idxCount = 0;


        RealButtonPanel = GameObject.Find("RealButtonPanel");
        TimePanel = GameObject.Find("TimePanel").GetComponent<Slider>();
        RealButtonPanel.SetActive(false);
    }

    void Start()
    {
        
    }

    void Update()
    {
        RenewTimeBar();
    }

    // 서버가 비올레타 테스트를 요청한 경우
    public void OnRecvVioleta(int var, float lv_)
    {
        long tm = GetMillisecondsSinceEpoch();
        byte[] seed = GetSeedValue(tm);

        m_time = tm;

        m_lv = lv_;

        for(int violetaNum = 0; violetaNum < 4; violetaNum++)
        {
            m_violeta[violetaNum].SetLevel(lv_);
        }

        StartCoroutine(m_violeta[var].ShowHat(0.5f));

        CalculateVioleta(seed);
    }
    // 시드값을 바탕으로 각 비올레타에 명령을 내립니다.
    // 각 비올레타의 명령큐에 들어간 후 비올레타가 움직이기 시작합니다.
    private void CalculateVioleta(byte[] seed)
    {
        int[] res1 = new int[4], res2 = new int[4];

        for (int epoch = 0; epoch < seed.Length; epoch += 8)
        {
            for(int lhs = 0; lhs < 3; lhs++)
            {
                for (int rhs = lhs + 1; rhs < 4; rhs++)
                {
                    byte lhsVal = (byte)(seed[epoch + lhs] ^ seed[epoch + lhs + 4]), rhsVal = (byte)(seed[epoch + rhs] ^ seed[epoch + rhs + 4]);

                    if (lhsVal < rhsVal)
                    {
                        res1[rhs]++;
                    }
                    else
                    {
                        res1[lhs]++;
                    }

                    int a = ((lhsVal % 16) * 16) + (lhsVal / 16);
                    int b = ((rhsVal % 16) * 16) + (rhsVal / 16);

                    if (a < b)
                    {
                        res2[rhs]++;
                    }
                    else
                    {
                        res2[lhs]++;
                    }
                }
            }

            for(int violetaNum = 0; violetaNum < 4; violetaNum++)
            {
                m_violeta[violetaNum].EnQueueData(res1[violetaNum], res2[violetaNum]);
            }

            res1[0] = res1[1] = res1[2] = res1[3] = 0;
            res2[0] = res2[1] = res2[2] = res2[3] = 0;
        }

        CommandVioleta();
        StartCoroutine(SetButtonActive());
    }

    private void CommandVioleta()
    {
        for(int violetaNum = 0; violetaNum < 4; violetaNum++)
        {
            m_violeta[violetaNum].MoveVioleta();
        }
    }

    private byte[] GetSeedValue(long tm_)
    {
        byte[] hash = ComputeSHA256Hash(tm_.ToString());

        string hexString = BitConverter.ToString(hash).Replace("-", " ");
        Debug.Log(tm_ + " hash:" + hexString);

        for(int epoch = 0; epoch < hash.Length; epoch++)
        {
            hash[epoch] = Sbox[hash[epoch]];
        }

        hexString = BitConverter.ToString(hash).Replace("-", "");
        Debug.Log(tm_ + " sbox:" + hexString);

        return hash;
    }

    private long GetMillisecondsSinceEpoch()
    {
        // Unix epoch (1970년 1월 1일 00:00:00 UTC)
        DateTime epoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        // 현재 UTC 시간
        DateTime now = DateTime.UtcNow;

        // 두 시간 간의 차이를 구합니다
        TimeSpan elapsed = now - epoch;

        // 밀리초 단위로 변환합니다
        long milliseconds = (long)elapsed.TotalMilliseconds;

        return milliseconds;
    }

    private byte[] ComputeSHA256Hash(string input)
    {
        // SHA-256 해시 객체 생성
        using (SHA256 sha256 = SHA256.Create())
        {
            // 입력 문자열을 바이트 배열로 변환
            byte[] bytes = Encoding.UTF8.GetBytes(input);

            // 바이트 배열을 SHA-256 해시로 변환
            byte[] hashBytes = sha256.ComputeHash(bytes);

            return hashBytes;
        }
    }

    public void WriteAnswer(int answer_)
    {
        m_answer[m_idxCount++] = answer_;
        m_timeleft = epochTime + 10f;

        if (m_idxCount == 4)
        {
            SendMsg();
            m_idxCount = 0;
        }
        else
        {
            CommandVioleta();
            StartCoroutine(SetButtonActive());
        }
    }

    private IEnumerator SetButtonActive()
    {
        yield return new WaitForSeconds(m_lv * 3 + 0.1f);

        m_timeleft = epochTime;

        RealButtonPanel.SetActive(true);
    }

    private void RenewTimeBar()
    {
        if(m_timeleft > epochTime)
        {
            TimePanel.value = 1f;
            m_timeleft -= Time.deltaTime;
        }
        else if(m_timeleft > 0)
        {
            TimePanel.value = m_timeleft / epochTime;

            m_timeleft -= Time.deltaTime;
        }
        else
        {
            // time fault -> send res to server
        }
    }

    void SendMsg()
    {
        string msg = m_answer[0].ToString() + m_answer[1].ToString() + m_answer[2].ToString() + m_answer[3].ToString() + m_time.ToString();

        NetworkManager.m_instance.SendMessageToServer(msg);

        transform.parent.gameObject.SetActive(false);
    }
}
