using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

public class Violeta : MonoBehaviour
{
    private Animator _animator;
    private Image _image;
    private SpriteRenderer _spriteRenderer;
    private float _level;
    private long m_Hat_time;

    private Queue<Tuple<int,int>> _Nexts; // 0~3, 0~3

    void Awake()
    {
        _animator = GetComponent<Animator>();
        _image = GetComponent<Image>();
        _spriteRenderer = GetComponent<SpriteRenderer>();
        _Nexts = new Queue<Tuple<int,int>>();
    }

    // Update is called once per frame
    void Update()
    {
        _image.sprite = _spriteRenderer.sprite;
    }

    public void MoveVioleta()
    {
        if(_Nexts.Count == 0)
        {
            Debug.Log("Violeta::MoveVioleta : EmptyQueue.");
            return;
        }

        StartCoroutine(Move(_level));
    }

    public void SetLevel(float lv_) { _level = lv_; }

    private IEnumerator Move(float time_)
    {
        Tuple<int,int> next = _Nexts.Dequeue();

        int row = next.Item1, col = next.Item2;

        // Move Vertically
        Vector2 endPosition = new Vector2(transform.localPosition.x, -100 + row * 200 / 3);
        _animator.SetBool("Moving", true);
        yield return MoveToPosition(endPosition, time_);

        // Move Horizontally
        endPosition = new Vector2(-400 + col * 800 / 3, transform.localPosition.y);
        yield return MoveToPosition(endPosition, time_);

        // Move Vertically
        endPosition = new Vector2(transform.localPosition.x, 0);
        yield return MoveToPosition(endPosition, time_);
        _animator.SetBool("Moving", false);
    }

    private IEnumerator MoveToPosition(Vector2 endPosition_, float time_)
    {
        Vector2 startPosition = transform.localPosition;
        float elapsedTime = 0;

        while (elapsedTime < time_)
        {
            transform.localPosition = Vector2.Lerp(startPosition, endPosition_, (elapsedTime / time_));
            elapsedTime += Time.deltaTime;
            yield return null;
        }

        transform.localPosition = endPosition_;
    }

    public IEnumerator ShowHat(float time_)
    {
        Image hat = transform.GetChild(0).GetComponent<Image>();
        float elapsedTime = 0;

        while(elapsedTime < time_)
        {
            hat.color = new Color(hat.color.r, hat.color.g, hat.color.b, (time_ - elapsedTime) / time_);
            elapsedTime += Time.deltaTime;
            yield return null;
        }

        hat.color = new Color(hat.color.r, hat.color.g, hat.color.b, 0f);
    }

    public void EnQueueData(int a, int b)
    {
        _Nexts.Enqueue(Tuple.Create(a, b));
    }
}
