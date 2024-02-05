# pragma omp task
{
    ntask1 = func();

    // submit
    for (i = 0 ; i < ntask1 ; ++i)
    {
        # pragma omp task
        {

        }
    }


    // ntask peut avoir changé
    ntask2 = func();
}

# pragma omp task
{
    // submit
    for (i = 0 ; i < ntask2 ; ++i)
    {
        # pragma omp task
        {
        }
    }


}
